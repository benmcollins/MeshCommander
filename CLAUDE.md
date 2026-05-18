# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

QuMesh — a native Qt6/QML console for managing Intel AMT (vPro) hardware over WSMAN and the Intel redirection protocol. Cross-platform (macOS + Windows). The successor to Intel's NW.js MeshCommander, which lived under `legacy/` in this repo until it was deleted (see git history, or Intel's upstream at https://github.com/Ylianst/MeshCommander, for the reference implementation).

## Workflow

The default branch is `main`. Open PRs against `main` and merge into `main`.

For any non-trivial change in this repo, follow these steps in order:

1. **Create an issue** describing the change.
2. **Create a branch** off `main` named `<issue#>-<short-desc>` (e.g. `97-ssh-tunnel`).
3. **Write the implementation plan** — the approach, files you'll touch, what's in scope and what isn't.
4. **Comment the plan on the issue** before writing code, so the maintainer can redirect early.
5. **Implement the plan** on the branch. Tests are required for all new code. CI (`.github/workflows/ci.yml`) builds and tests on macOS + Windows on every PR.
6. **Open the PR** when the implementation is complete and passes locally.
7. **Watch CI** until both `macos-latest` and `windows-latest` are green.
8. **Merge** once CI is green.

For Qt/QML changes specifically: use the `qt-development-skills` plugin — invoke `qt-development-skills:qt-qml` when authoring QML, run `qt-development-skills:qt-qml-review` and/or `qt-development-skills:qt-cpp-review` before opening a PR.

## Build

```
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the app: `./build/app/qumesh.app/Contents/MacOS/qumesh` (macOS) or `build\app\qumesh.exe` (Windows). Requires Qt 6.6+ with the `Quick`, `QuickControls2`, `Network`, and `Test` modules — on macOS install via `brew install qt ninja`. CI uses Qt 6.8.0.

## Legacy MeshCommander persistence (load-bearing for `migrate/`)

The `migrate/` module reads configuration written by the legacy NW.js MeshCommander off user disks. That storage is still in the field, so the format details below matter even though the legacy source tree is gone.

MeshCommander stored everything in browser `localStorage`, persisted on disk at:

```
~/Library/Application Support/meshcommander/Default/Local Storage/leveldb/
```

…keyed under the `chrome-extension://<id>` origin that NW.js derived from the legacy `package.json`'s `name: "meshcommander"`. Two values matter:

- `computers` — the AMT machine list, encrypted JSON.
- `certificates` — the cert store.

Two encryption envelopes coexist:

- **Legacy** — `crypto.createCipher('aes-256-ctr', password)`, which derived key+IV via OpenSSL `EVP_BytesToKey(MD5, no salt, 1 iter)`. Used by MeshCommander builds on Node < 22.
- **v2** — `crypto.createCipheriv` with a random key/IV, tagged with a `v2:` prefix. Used by later MeshCommander builds after `createCipher` was removed in Node 22.

`migrate/src/legacy_decrypt.cpp` reads both. Don't simplify away the legacy path — users importing from old installs need it.
