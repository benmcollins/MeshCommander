#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>
#
# Tests for release-body.sh. Runs in CI on every PR; release.yml itself
# only fires on `v*.*.*` tags, so without this the release body is
# unexercised until it is already published (#443).
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UNDER_TEST="$SCRIPT_DIR/release-body.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

failures=0
checked=0

# assert_contains <label> <haystack-file> <needle>
assert_contains() {
    checked=$((checked + 1))
    if grep -qF -- "$3" "$2"; then
        echo "  ok   — $1"
    else
        echo "  FAIL — $1"
        echo "         expected to find: $3"
        failures=$((failures + 1))
    fi
}

# assert_absent <label> <haystack-file> <needle>
assert_absent() {
    checked=$((checked + 1))
    if grep -qF -- "$3" "$2"; then
        echo "  FAIL — $1"
        echo "         expected NOT to find: $3"
        failures=$((failures + 1))
    else
        echo "  ok   — $1"
    fi
}

cat > "$TMP/notes.md" <<'NOTES'
# QuMesh 9.9.9

Lead paragraph of the notes.

## A section heading

Body text under the section.
NOTES

run_body() {
    env TAG="${TAG:-v9.9.9}" PREV_TAG="${PREV_TAG-v9.9.8}" \
        NOTES_MD_FILE="$TMP/notes.md" \
        MAC_NOTARIZED="${MAC_NOTARIZED:-false}" \
        WIN_SIGNED="${WIN_SIGNED:-false}" \
        GITHUB_SERVER_URL="https://github.com" \
        GITHUB_REPOSITORY="benmcollins/QuMesh" \
        bash "$UNDER_TEST" > "$1"
}

echo "== macOS signed + notarized (the state since v1.1.0) =="
MAC_NOTARIZED=true run_body "$TMP/out"
assert_contains "says notarized" "$TMP/out" "notarized by Apple"
assert_absent   "no unsigned claim" "$TMP/out" "the bundle is unsigned"
assert_absent   "no Gatekeeper bypass advice" "$TMP/out" "right-click"

echo "== macOS unsigned (fallback if the secret is ever removed) =="
MAC_NOTARIZED=false run_body "$TMP/out"
assert_contains "warns unsigned" "$TMP/out" "Gatekeeper will warn that the bundle is unsigned"
assert_absent   "does not claim notarization" "$TMP/out" "notarized by Apple"

echo "== Windows unsigned (the state today) =="
WIN_SIGNED=false run_body "$TMP/out"
assert_contains "warns unverified publisher" "$TMP/out" "publisher is unverified"
assert_absent   "does not claim Authenticode" "$TMP/out" "Authenticode signed"

echo "== Windows signed (self-corrects when a cert is provisioned) =="
WIN_SIGNED=true run_body "$TMP/out"
assert_contains "says Authenticode signed" "$TMP/out" "It is Authenticode signed"
assert_absent   "drops the unverified warning" "$TMP/out" "publisher is unverified"

echo "== changelog link =="
PREV_TAG=v1.4.0 TAG=v1.4.1 run_body "$TMP/out"
assert_contains "compare link carries both refs" "$TMP/out" \
    "https://github.com/benmcollins/QuMesh/compare/v1.4.0...v1.4.1"
assert_absent "never emits a base-less compare" "$TMP/out" "compare/...v"

PREV_TAG='' run_body "$TMP/out"
assert_absent "omitted on the first-ever release" "$TMP/out" "Full changelog"

PREV_TAG=v9.9.9 TAG=v9.9.9 run_body "$TMP/out"
assert_absent "omitted when prev == current" "$TMP/out" "Full changelog"

echo "== notes body =="
run_body "$TMP/out"
assert_contains "lead paragraph survives" "$TMP/out" "Lead paragraph of the notes."
assert_contains "later sections survive" "$TMP/out" "Body text under the section."
assert_absent   "leading H1 stripped (title is already shown above)" "$TMP/out" "# QuMesh 9.9.9"
assert_contains "sub-headings are NOT stripped" "$TMP/out" "## A section heading"
checked=$((checked + 1))
if [ "$(head -1 "$TMP/out")" = "Lead paragraph of the notes." ]; then
    echo "  ok   — body opens on the notes, with no leading blank"
else
    echo "  FAIL — body opens on: $(head -1 "$TMP/out")"
    failures=$((failures + 1))
fi

# A notes file with no leading H1 must not lose its first line.
printf 'No heading here.\n\nSecond para.\n' > "$TMP/notes.md"
run_body "$TMP/out"
assert_contains "first line kept when there is no H1" "$TMP/out" "No heading here."

echo "== Linux APT block stays literal =="
# shellcheck disable=SC2016  # the literal text is the point
assert_contains "shell vars not expanded by the heredoc" "$TMP/out" '${ID}${VERSION_ID}'
# shellcheck disable=SC1003  # matching a literal trailing backslash
assert_contains "line continuations preserved" "$TMP/out" 'apt-pub.asc | \'
assert_contains "install heading present" "$TMP/out" "## Install"

echo
if [ "$failures" -eq 0 ]; then
    echo "All $checked assertions passed."
else
    echo "$failures of $checked assertions FAILED."
fi
exit "$((failures > 0))"
