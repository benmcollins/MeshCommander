# QuMesh

QuMesh is a native Qt6/QML console for Intel AMT / vPro devices, in active development. Cross-platform (macOS + Windows), with a one-shot importer for configurations saved by the legacy NW.js MeshCommander.

See [`ROADMAP.md`](ROADMAP.md) for the phased plan and [`CLAUDE.md`](CLAUDE.md) for how to build and where the pieces live.

## Build

```
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build
ctest --test-dir build --output-on-failure
```

## Legacy

The original `MeshCommander` NW.js app (Intel's, repackaged for macOS) lives under `legacy/` (`legacy/source/`, `legacy/mkapp*`, `legacy/files/`). It's the reference implementation and is being removed progressively as Qt replacements land — see `ROADMAP.md` ("Progressive cleanup").
