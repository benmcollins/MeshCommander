#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>
#
# Assemble the GitHub Release body and write it to stdout.
#
# Lives here rather than inline in release.yml so it can be tested:
# release.yml only fires on `v*.*.*` tags, so a defect in this text is
# otherwise discovered by users reading a published release. That is
# exactly how #443 happened — every release from v1.1.0 to v1.4.1 told
# macOS users to right-click → Open past a Gatekeeper prompt that a
# signed, notarized bundle never raises.
#
# Inputs (environment):
#   TAG                 release tag, e.g. v1.4.1
#   PREV_TAG            previous release tag, or empty on the first release
#   NOTES_MD_FILE       path to the resolved Markdown release notes
#   MAC_NOTARIZED       "true" when the macOS signing secrets are provisioned
#   WIN_SIGNED          "true" when the Authenticode secret is provisioned
#   GITHUB_SERVER_URL   e.g. https://github.com
#   GITHUB_REPOSITORY   e.g. benmcollins/QuMesh
set -euo pipefail

: "${TAG:?TAG is required}"
: "${NOTES_MD_FILE:?NOTES_MD_FILE is required}"
PREV_TAG="${PREV_TAG:-}"
MAC_NOTARIZED="${MAC_NOTARIZED:-false}"
WIN_SIGNED="${WIN_SIGNED:-false}"
GITHUB_SERVER_URL="${GITHUB_SERVER_URL:-https://github.com}"
GITHUB_REPOSITORY="${GITHUB_REPOSITORY:-benmcollins/QuMesh}"

# GitHub renders the release title above the body, so the notes file's
# own leading `# QuMesh <version>` heading would print the version
# twice. Drop it, plus any blank lines it left behind.
NOTES_BODY=$(awk 'NR==1 && /^#[^#]/ { next } !seen && NF == 0 { next } { seen=1; print }' "$NOTES_MD_FILE")

printf '%s\n\n' "$NOTES_BODY"
echo '## Install'
echo

# Both platform lines are derived from whether the signing secrets are
# provisioned, never hardcoded. The Notarize step in release.yml has no
# continue-on-error, so with MAC_NOTARIZE_APPLE_ID set the macOS build
# job fails before publish if notarization fails — a published release
# is therefore necessarily notarized, which makes the signed wording
# accurate by construction rather than by someone remembering to
# update it. Windows is unsigned today; its line self-corrects when an
# Authenticode cert is provisioned.
if [ "$MAC_NOTARIZED" = "true" ]; then
    cat <<'MACEOF'
- **macOS** — drag QuMesh.app from the .dmg to Applications. The bundle
  is Developer ID signed, notarized by Apple, and the notarization
  ticket is stapled to the .dmg, so it opens normally on first launch.
MACEOF
else
    cat <<'MACEOF'
- **macOS** — drag QuMesh.app from the .dmg to Applications. On first
  launch Gatekeeper will warn that the bundle is unsigned; right-click →
  Open to bypass.
MACEOF
fi

if [ "$WIN_SIGNED" = "true" ]; then
    cat <<'WINEOF'
- **Windows** — run the .exe installer. It is Authenticode signed; if
  SmartScreen still prompts on a freshly published build, click "More
  info" → "Run anyway".
WINEOF
else
    cat <<'WINEOF'
- **Windows** — run the .exe installer. SmartScreen will warn that the
  publisher is unverified; click "More info" → "Run anyway".
WINEOF
fi

cat <<'LINUXEOF'
- **Linux (Debian 13 / Ubuntu 26.04, amd64 or arm64)** — preferred
  path is the APT repo so apt handles upgrades. The snippet picks
  the right suite (`ubuntu26.04` / `debian13`) from
  `/etc/os-release`:

      . /etc/os-release
      SUITE="${ID}${VERSION_ID}"

      curl -fsSL https://benmcollins.github.io/QuMesh/apt-pub.asc | \
          sudo tee /etc/apt/keyrings/qumesh.asc > /dev/null
      echo "deb [signed-by=/etc/apt/keyrings/qumesh.asc] \
          https://benmcollins.github.io/QuMesh $SUITE main" | \
          sudo tee /etc/apt/sources.list.d/qumesh.list
      sudo apt update && sudo apt install qumesh

  Or grab the matching `.deb` below and `sudo apt install ./qumesh_*.deb`.
LINUXEOF

# Omitted entirely on the first release: with no base ref the link
# renders as `compare/...v1.0.0`, which is a 404 (#443).
if [ -n "$PREV_TAG" ] && [ "$PREV_TAG" != "$TAG" ]; then
    printf '\n[Full changelog](%s/%s/compare/%s...%s)\n' \
        "$GITHUB_SERVER_URL" "$GITHUB_REPOSITORY" "$PREV_TAG" "$TAG"
fi
