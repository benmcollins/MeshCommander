# QuMesh — Roadmap

QuMesh is a native Qt6 + QML console for Intel AMT devices, replacing the legacy NW.js-wrapped MeshCommander (`source/Commander.htm`). Cross-platform (macOS + Windows), one-shot import from the legacy app's saved config.

## Why

The legacy app is a 1.1 GB NW.js bundle wrapping a ~50k-line single-page web app. It's fragile against Node/Chromium API churn (see the `crypto.createCipher` removal that broke saved config), the bundle is enormous, and the codebase is hard to extend. A native Qt app removes the Chromium dependency, gives proper desktop integration, and lets us evolve the UI without touching a 3 MB HTML file.

## Phases

Each phase is one or more issues. The workflow per logical unit is: issue → branch (`<issue#>-<short-desc>`) → PR → merge.

### Phase 0 — Foundation (this issue)

- `qt/` project skeleton, CMake, ROADMAP
- Qt+QML minimal app that opens a window
- QTest smoke test
- GitHub Actions CI: macOS + Windows matrix

### Phase 1 — Config compatibility

- **Migration tool** reading Chromium's leveldb localStorage at the legacy app's data dir, decrypting `computers` and `certificates` with the v2/legacy AES envelope (see `source/Commander.htm` for the format we patched, and the [legacy storage memory](../../../) for reference), and writing a native config under the new app's data directory.
- Tests with synthetic leveldb fixtures + a real captured fixture from the legacy app.

### Phase 2 — Computer-list UI

- QML list view bound to a C++ `QAbstractListModel`.
- Add/edit/delete computer dialogs.
- Persistence in the native config format from Phase 1.
- Cert-pinning UI hook (no AMT connection yet).

### Phase 3 — AMT WSMAN client

- HTTP/HTTPS digest auth.
- SOAP envelope generation for CIM/AMT classes.
- Connection test, hardware inventory, event log retrieval, power state read, power state change.
- Backed by a C++ library, exposed to QML through a small model API.

### Phase 4 — AMT Redirection transport

- Authenticated TLS connection on port 16994/16995.
- Auth/version negotiation (Intel's binary framing — port over from `source/amt-redir-*.js`).
- Generic redirection-channel object that SOL/IDE-R/KVM build on.

### Phase 5 — SOL terminal

- VT100 terminal widget (QML, with C++ ANSI parser). ✅ (#19)
- SOL channel on top of the redirection transport. ✅ (#17)

### Phase 6 — IDE-R

- Mount local ISO as remote IDE/USB. ✅ (#21)
- IDE-R framing on top of the redirection transport. ✅ (#21)

### Phase 7 — KVM

- KVM channel decoder (Intel's RFB-variant — port from `source/amt-desktop-*.js`). ✅ (#23)
- QML viewer with input forwarding. ✅ (#23)

### Phase 8 — Certificate store

- Import/export from the legacy migrated state. ✅ (#25)
- Mutual-TLS configuration for AMT TLS. (Deferred — lands with the TLS-on-16995 follow-up.)

### Phase 9 — Packaging

- macOS `.app` bundle with codesigning + notarization. ✅ unsigned bundle (#27)
- Windows installer (WiX or NSIS). ✅ NSIS .exe (#27)

Signing and notarization are gated on repo secrets (see
[PACKAGING.md](PACKAGING.md)) and activate automatically once those
land. The unsigned bundles still run; users see the OS's standard
"unverified publisher" prompt on first launch.

## Reference

The legacy implementation in `source/` is the canonical reference for AMT behavior. When implementing a feature, the corresponding `source/amt-*.js` module is authoritative. The forge.js library handles all the crypto / certificate operations — its C++ equivalent in this rewrite is OpenSSL via Qt or directly.

## Progressive cleanup

Each phase PR must also delete the legacy files it replaces. Example: the WSMAN phase removes `source/amt-wsman-*.js`; the redirection phase removes `source/amt-redir-*.js`; etc. `source/Commander.htm` is monolithic, so it's deleted only when the Qt UI is feature-complete. The packaging phase (final) moves `qt/*` up to the repo root and removes the legacy build tooling (`mkapp*`, top-level `package.json`, `files/`).
