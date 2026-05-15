# QuMesh logo assets

Cross-platform icon assets for the QuMesh application (macOS + Windows, Qt/QML).

## Source files (`svg/`)

| File | Purpose |
| --- | --- |
| `qumesh-mark.svg` | Master mark on transparent background. Uses `currentColor` so the fill follows the surrounding text/icon color in Qt and CSS. |
| `qumesh-mark-small.svg` | Simplified variant (Q + tail node, no interior lattice). Used for renders ≤ 32 px where the lattice detail muddies. Also uses `currentColor`. |
| `qumesh-macos.svg` | Slate squircle tile (1024×1024) with white mark. The source for `.icns`. |
| `qumesh-windows.svg` | Slate mark on transparent background (256×256). The source for `.ico`. |
| `qumesh-og.svg` | 1280×640 social preview / Open Graph card. Renders to `build/og-image.png`; upload that file at **Repo → Settings → Social preview** so GitHub serves it as the OG image when the repo URL is shared on Twitter, Slack, Discord, etc. |

Brand color: **#1e293b** (slate-800), accent **#38bdf8** (sky-400).

## Building the icon bundles

```bash
pip install cairosvg pillow
python build-icons.py
```

Outputs:

```
build/
├── qumesh.icns                # macOS application icon
├── qumesh.ico                 # Windows application icon
├── og-image.png               # 1280×640 GitHub social preview
├── qumesh.iconset/            # intermediate macOS iconset
└── png/                       # standalone PNG renders at standard sizes
    ├── qumesh-mark-16.png
    ├── qumesh-mark-32.png
    ├── …
    ├── qumesh-macos-128.png
    └── …
```

On macOS, the script uses the system `iconutil` for an optimized `.icns`. On Linux or Windows build hosts, it falls back to a Pillow-based ICNS writer that produces a functional but larger file.

For `og-image.png` to render the wordmark in IBM Plex Sans (matching the app), make the bundled font visible to fontconfig before running the script:

```bash
mkdir -p ~/.local/share/fonts/qumesh-build
cp ../app/qml/fonts/IBMPlexSans-{Regular,Medium}.ttf ~/.local/share/fonts/qumesh-build/
fc-cache -f ~/.local/share/fonts/qumesh-build
```

Without that step the script still produces a usable card — the SVG falls back to Helvetica Neue / Arial.

### Uploading the social preview to GitHub

GitHub's social preview image is only settable through the web UI; there's no `gh` API endpoint for it. Once `og-image.png` is generated:

1. Open **Repo → Settings → General**.
2. Under **Social preview**, click **Edit** → **Upload an image**.
3. Pick `branding/build/og-image.png`.

GitHub then serves it as the Open Graph image when the repo URL is unfurled in Twitter, LinkedIn, Slack, Discord, iMessage, etc.

## Qt/QML integration

Include the resource file in your CMake or qmake build:

**CMake:**

```cmake
qt_add_resources(qumesh_icons "qumesh-icons.qrc")
target_sources(QuMesh PRIVATE ${qumesh_icons})
```

**qmake:**

```pro
RESOURCES += qumesh-icons.qrc
```

### Window icon (C++ / `main.cpp`)

```cpp
#include <QGuiApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/qumesh-mark.svg"));
    // …
}
```

### QML usage

```qml
import QtQuick

Image {
    source: "qrc:/icons/qumesh-mark.svg"
    sourceSize.width: 32
    sourceSize.height: 32
}
```

For QML icons that should tint with the system theme, the SVG's `currentColor` won't work directly — use `ColorOverlay` from `Qt5Compat.GraphicalEffects` or the newer `MultiEffect` in Qt 6.5+.

## Application packaging

### macOS (`.app` bundle)

Place `qumesh.icns` in `Contents/Resources/` and reference it in `Info.plist`:

```xml
<key>CFBundleIconFile</key>
<string>qumesh.icns</string>
```

CMake can do this automatically:

```cmake
set_target_properties(QuMesh PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE qumesh.icns
)
set_source_files_properties(build/qumesh.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)
target_sources(QuMesh PRIVATE build/qumesh.icns)
```

### Windows (`.exe`)

Reference `qumesh.ico` in a Windows resource file (`qumesh.rc`):

```
IDI_ICON1 ICON "build/qumesh.ico"
```

And include it in your build:

```cmake
if(WIN32)
    target_sources(QuMesh PRIVATE qumesh.rc)
endif()
```
