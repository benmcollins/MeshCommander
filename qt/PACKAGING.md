<!--
SPDX-License-Identifier: Apache-2.0
Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>
-->

# Packaging QuMesh

QuMesh ships as a `.dmg` for macOS and an NSIS-based `.exe` installer for
Windows. Both bundles are self-contained — Qt, the QML modules, and
OpenSSL are inlined so the app runs on a fresh OS install without
needing developer tooling.

## Cutting a release

1. Bump `PROJECT_VERSION` at the top of `qt/CMakeLists.txt`.
2. Commit and push, then tag:
   ```
   git tag v0.1.0
   git push origin v0.1.0
   ```
3. The `Release` GitHub Actions workflow builds both platforms, runs
   the test suite, packages with CPack, and publishes a draft GitHub
   Release with the two artifacts attached.

## Local packaging (smoke test)

```
cd qt
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build
cpack -G DragNDrop      # macOS
cpack -G NSIS           # Windows
```

The output lands in `qt/build/`. Note: Homebrew Qt installs everything
as symlinks back into `/opt/homebrew/Cellar`, so a locally built DMG
will reference those paths and won't run on another machine. Use the
CI artifact for actual distribution.

## Signing and notarization

Both platforms ship unsigned by default. The bundle still runs, but
users see the platform's standard "unverified publisher" warning on
first launch. Provisioning the secrets below activates signing and (on
macOS) notarization without any further code changes.

### macOS

| Secret                     | Source |
|----------------------------|--------|
| `MAC_SIGNING_IDENTITY`     | The Developer ID Application identity string, e.g. `Developer ID Application: Your Name (TEAMID)`. Run `security find-identity -v -p codesigning` on the build machine to list candidates. |
| `MAC_NOTARIZE_APPLE_ID`    | The Apple ID associated with your developer account. |
| `MAC_NOTARIZE_TEAM_ID`     | Your 10-character Apple Developer Team ID. |
| `MAC_NOTARIZE_PASSWORD`    | An app-specific password generated at <https://appleid.apple.com> (Sign-In and Security → App-Specific Passwords). |

Add them under **Settings → Secrets and variables → Actions** in this
repo. The CMake install step picks up `MAC_SIGNING_IDENTITY` and signs
the bundle with hardened-runtime entitlements; the release workflow
follows up with `notarytool submit --wait` and `stapler staple`.

If you also have an Apple Developer ID **Installer** certificate and
want a signed `.pkg` instead of a `.dmg`, switch CPack to the
`productbuild` generator in `qt/CMakeLists.txt` and add
`MAC_INSTALLER_IDENTITY` plumbing — out of scope for v1.

### Windows

| Secret                  | Source |
|-------------------------|--------|
| `WIN_SIGN_THUMBPRINT`   | SHA-1 thumbprint of a code-signing certificate already installed in the GitHub-hosted runner's certificate store. Install the cert in a runner-setup step (or move to a self-hosted runner) and pass the thumbprint here. |

The CMake install step invokes `signtool sign /sha1 <thumbprint>` with
SHA-256 file digest and a DigiCert timestamp.

For EV certificates that require a USB token, the runner must be
self-hosted (the token can't be plugged into a GitHub-hosted VM).

## Translations

User-facing strings are wrapped in `qsTr(...)` and extracted by
`lupdate` into `qt/app/translations/qumesh_<locale>.ts`. The `app/`
CMake target calls `qt_add_translations()` to wire this up:

- `cmake --build build --target update_translations` runs `lupdate`
  to refresh the `.ts` files with any new strings. Edit the `.ts`
  files (manually or in Qt Linguist) to add translations.
- `cmake --build build --target release_translations` compiles the
  `.ts` files into `.qm` and embeds them under `qrc:/i18n/` in the
  app's QML resource module. The release target runs automatically
  as part of the normal build.
- At launch, `main.cpp` installs a `QTranslator` that loads
  `qumesh_<system locale>.qm` if present, falling back to the
  un-translated English source baked into the `qsTr()` calls.

To add a new language:

1. Append `translations/qumesh_<locale>.ts` to the `TS_FILES` list
   in `qt/app/CMakeLists.txt`.
2. Run the `update_translations` target — it will create the file
   and seed it from the source strings.
3. Translate; rebuild.

## What the bundle contains

- `QuMesh.app/Contents/MacOS/QuMesh` (or `QuMesh.exe`).
- The Qt frameworks linked at build time, copied by `macdeployqt` /
  `windeployqt` from the Qt install the build used.
- The QML modules QuMesh imports (`QtQuick`, `QtQuick.Controls.Basic`,
  `QtQuick.Layouts`, `QtQuick.Dialogs`, etc.), discovered via
  `qmlimportscanner`.
- `libcrypto-3-x64.dll` on Windows (Qt's deploy tool doesn't follow
  non-Qt DLL dependencies).
- The bundled fonts (IBM Plex Sans, JetBrains Mono).
- The Apache 2.0 license text.
