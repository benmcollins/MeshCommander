<!--
SPDX-License-Identifier: Apache-2.0
Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>
-->

# Bundled fonts

Bundled so the app renders identically across macOS, Windows, and Linux
without relying on whatever the host has installed.

| Font | Files | License | Source |
| --- | --- | --- | --- |
| IBM Plex Sans | `IBMPlexSans-Regular.ttf`, `IBMPlexSans-Medium.ttf` | SIL OFL 1.1 — see `IBMPlex-OFL.txt` | <https://github.com/IBM/plex> |
| JetBrains Mono | `JetBrainsMono-Regular.ttf` | SIL OFL 1.1 — see `JetBrainsMono-OFL.txt` | <https://github.com/JetBrains/JetBrainsMono> |

Used by `qml/Theme/Type.qml` and exposed to the rest of the QML tree via
the `Type.sans` and `Type.mono` singleton properties.
