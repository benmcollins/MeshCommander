#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>
#
# Sparkle-aware Developer ID code-sign sequence for the QuMesh bundle
# on macOS. Invoked from `app/CMakeLists.txt` at install time when
# MAC_SIGNING_IDENTITY is provisioned. Runs after macdeployqt has
# bundled Qt frameworks + plugins, and after Sparkle.framework has
# been copied into Contents/Frameworks/.
#
# `codesign --deep` alone doesn't satisfy Sparkle 2's nested-bundle
# rules — the XPCServices, Autoupdate, and Updater.app inside
# Sparkle.framework must be signed individually before the framework
# wrapper, and the app shell must carry our hardened-runtime
# entitlements. This script handles that order.

set -euo pipefail

if [ $# -ne 3 ]; then
    cat >&2 <<USAGE
Usage: codesign-bundle.sh <app-bundle> <signing-identity> <entitlements>

  <app-bundle>       Path to QuMesh.app
  <signing-identity> codesign --sign argument (e.g. "Developer ID Application: …")
  <entitlements>     Path to the .entitlements plist for the app shell
USAGE
    exit 2
fi

APP="$1"
IDENTITY="$2"
ENTITLEMENTS="$3"

if [ ! -d "$APP" ]; then
    echo "codesign-bundle: app bundle not found at $APP" >&2
    exit 1
fi
if [ ! -f "$ENTITLEMENTS" ]; then
    echo "codesign-bundle: entitlements not found at $ENTITLEMENTS" >&2
    exit 1
fi

# `--options runtime` enables the hardened runtime; `--timestamp`
# stamps with Apple's TSA, which notarization requires. `--force`
# replaces any prior signature (Qt frameworks arrive pre-signed by Qt,
# Sparkle pre-signed by the Sparkle team — both need our identity).
sign_one() {
    codesign --force --options runtime --timestamp \
        --sign "$IDENTITY" "$1"
}

# 1. Sparkle.framework inner-out. The framework's nested helpers must
#    be signed before the framework wrapper, otherwise codesign refuses
#    to seal the outer bundle ("nested code is not signed").
SPARKLE_FW="$APP/Contents/Frameworks/Sparkle.framework"
if [ -d "$SPARKLE_FW" ]; then
    SV="$SPARKLE_FW/Versions/B"
    for xpc in "$SV"/XPCServices/*.xpc; do
        [ -e "$xpc" ] && sign_one "$xpc"
    done
    [ -f "$SV/Autoupdate" ] && sign_one "$SV/Autoupdate"
    [ -d "$SV/Updater.app" ] && sign_one "$SV/Updater.app"
    sign_one "$SPARKLE_FW"
fi

# 2. Qt frameworks deposited by macdeployqt. Each .framework gets its
#    own signature; the Sparkle framework is already done above.
if [ -d "$APP/Contents/Frameworks" ]; then
    find "$APP/Contents/Frameworks" -maxdepth 1 -name '*.framework' -type d \
        -not -path "$SPARKLE_FW" -print0 \
    | while IFS= read -r -d '' fw; do
        sign_one "$fw"
    done
fi

# 3. Loose dylibs and Qt plugins. macdeployqt drops plugins under
#    Contents/PlugIns/<category>/; both bare .dylib files and bundled
#    .framework dirs may live there.
for root in "$APP/Contents/Frameworks" "$APP/Contents/PlugIns"; do
    [ -d "$root" ] || continue
    find "$root" -name '*.dylib' -type f -print0 \
    | while IFS= read -r -d '' dl; do
        sign_one "$dl"
    done
done

# 4. App shell with entitlements. This must be last; codesign rejects
#    signing an outer bundle if any nested code lacks a signature.
codesign --force --options runtime --timestamp \
    --entitlements "$ENTITLEMENTS" \
    --sign "$IDENTITY" "$APP"

# 5. Verify. `--deep --strict` checks the whole nested tree; any helper
#    we missed or signed with the wrong options will fail here rather
#    than at notarytool submission, which has a much slower feedback
#    loop.
codesign --verify --deep --strict --verbose=2 "$APP"
