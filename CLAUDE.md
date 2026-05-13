# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A macOS repackaging of Intel's MeshCommander web app (a console for managing Intel AMT / vPro hardware) as a native NW.js desktop application. The actual UI is a single giant HTML page (`source/Commander.htm`, ~53k lines) that pulls in inlined and external JavaScript and runs inside a Chromium webview with Node.js access.

The upstream is Intel's MeshCommander source (Apache-2.0). The local fork adds macOS packaging (`mkapp` family scripts) and small JS patches to keep the app working on current NW.js / Node versions.

## Build

```
npm install                      # install nw-builder (top-level package.json)
./mkapp-arm64                    # build Apple Silicon .app under build/MeshCommander-osxarm64/
```

`mkapp-arm64` is the working build script:
- Pins NW.js to **0.84.0** specifically. NW.js >= 0.85 ships Node >= 22, which removed `crypto.createCipher`/`createDecipher`. Newer source paths use `createCipheriv` (see "Persistence" below) but watch for other deprecated APIs before bumping the pin.
- Runs `node -e ...` from inside `source/` so glob-expanded paths land directly under `app.nw/`. **Do not** pass an absolute `srcDir` to nw-builder — its `path.resolve(nwProjectDir, file)` becomes a no-op and you'll get an `ERR_FS_CP_EINVAL: src and dest cannot be the same` error.
- Ad-hoc codesigns the bundle (`codesign -s -`) so it launches locally without a Developer ID.

`mkapp` (the original Intel-targeted script) is currently broken: it uses `var NwBuilder = import('nw-builder')` and then `new NwBuilder(...)` on what is actually a Promise. Don't try to "fix" it incrementally without replacing the whole launcher.

There are **no tests** and no lint config in this repo.

## Architecture

### Two layers
- **Build tooling** at the top level: `mkapp-arm64`, the top-level `package.json` (just `nw-builder`), `files/` (icns + Info.plist source + `en.lproj`/`en_GB.lproj` localized strings).
- **NW.js app payload** in `source/`. `source/package.json` is the NW.js manifest. Its `main` is `commander.htm` (the case mismatch with `Commander.htm` works because macOS's default filesystem is case-insensitive).

### Inside the payload
- `Commander.htm` is the canonical full source. The `commander-min*.htm` files and `commander_<lang>.htm` files are minified and/or localized copies — patch `Commander.htm`, not its derivatives, unless you know you're regenerating them.
- `source/forge.js/` and `source/amt-*.js` are Intel's modules. The forge library is also inlined verbatim into the middle of `Commander.htm` (roughly lines 5348–41144). `Commander.htm` therefore has **mixed line endings historically** — the inlined forge block was LF-only and the rest CRLF; the repo has since been normalized to LF (see `Normalize line endings to LF` commit).
- `source/package.json` lists native deps (`ffi`, `ref`, `ref-array`, `ref-struct`) but `source/node_modules/` is **not** committed and `mkapp-arm64` does **not** install them. Pure-JS features (AMT web UI, WSMAN, WebSocket redirection) work without these; anything calling those native modules will fail at runtime. Don't try to `npm install` them on Apple Silicon — they don't build on modern Node.

### Persistence (load-bearing detail)
Configuration is **not** in `source/*.json`. MeshCommander stores everything in browser `localStorage`, persisted on disk at:

```
~/Library/Application Support/meshcommander/Default/Local Storage/leveldb/
```

…keyed under the `chrome-extension://<id>` origin that NW.js derives from `source/package.json`'s `name: "meshcommander"`. Two values matter:

- `computers` — the AMT machine list, encrypted JSON. Read by `loadComputers` / written by `saveComputers` in `Commander.htm`.
- `certificates` — the cert store. Same pattern via `cert_loadCertificates` / `cert_saveCertificates`.

Encryption envelope was migrated from the legacy `crypto.createCipher('aes-256-ctr', password)` (which derived key+IV via OpenSSL `EVP_BytesToKey(MD5, no salt, 1 iter)`) to `crypto.createCipheriv` with a random key/IV. The migration helpers (`_localCryptoEvpKey`, `_localCryptoEncrypt`, `_localCryptoDecrypt`) sit just above `saveComputers` and are shared by both the computer-list and certificate-store paths. New writes are tagged with a `v2:` prefix; load paths transparently read either format and re-save in `v2:` on first read.

If a user reports "my saved machines disappeared" after a build change, the first suspect is something invalidating that chrome-extension origin (changing `name` in `source/package.json`, changing the user-data dir name, etc.) — not the encryption.

### Crypto API conflicts to watch for
- `crypto.createCipher`/`createDecipher` were removed in Node 22. The Node usages in `Commander.htm` have all been replaced; the remaining `createCipher` references in the codebase are inside the bundled forge.js library (`forge.cipher.createCipher`) and are unrelated.
- `forge.js/` and the inlined forge code use forge's own cipher API — leave those alone.

## Working with the giant Commander.htm

- It's ~3 MB / 50k+ lines, mostly minified inline libraries. Use `grep -n` to find call sites before reading.
- The file holds both the HTML structure and the application JS as `<script>` blocks. The functions discussed above (`saveComputers`, `loadComputers`, `cert_*`, `_localCrypto*`) live in one of the trailing `<script>` blocks past line 44000.
- Don't reflow or re-indent en masse — line-ending and whitespace churn produced ~70k-line diffs before normalization; a careful edit should produce a diff measured in dozens of lines. If you see a multi-thousand-line diff for a small edit, suspect line endings (`git diff --ignore-cr-at-eol --stat` is the diagnostic).
