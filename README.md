<!--
SPDX-License-Identifier: Apache-2.0
Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>
-->

# QuMesh

QuMesh is a native Qt6/QML console for Intel AMT / vPro devices, in active development under `qt/`. Cross-platform (macOS + Windows), with a one-shot importer for configurations saved by the legacy NW.js MeshCommander.

See [`qt/ROADMAP.md`](qt/ROADMAP.md) for the phased plan and [`CLAUDE.md`](CLAUDE.md) for how to build and where the pieces live.

## Build

```
cd qt
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build
ctest --test-dir build --output-on-failure
```

## Legacy

The original `MeshCommander` NW.js app (Intel's, repackaged for macOS) still lives at the repo root in `source/`, `mkapp*`, and `files/`. It's the reference implementation and is being removed progressively as Qt replacements land — see `qt/ROADMAP.md` ("Progressive cleanup").
