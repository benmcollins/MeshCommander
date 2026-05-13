# QuMesh logo assets

Cross-platform icon assets for the QuMesh application (macOS + Windows, Qt/QML).

## Source files (`svg/`)

| File | Purpose |
| --- | --- |
| `qumesh-mark.svg` | Master mark on transparent background. Uses `currentColor` so the fill follows the surrounding text/icon color in Qt and CSS. |
| `qumesh-mark-small.svg` | Simplified variant (Q + tail node, no interior lattice). Used for renders ≤ 32 px where the lattice detail muddies. Also uses `currentColor`. |
| `qumesh-macos.svg` | Slate squircle tile (1024×1024) with white mark. The source for `.icns`. |
| `qumesh-windows.svg` | Slate mark on transparent background (256×256). The source for `.ico`. |

Brand color: **#1e293b** (slate-800).

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
├── qumesh.iconset/            # intermediate macOS iconset
└── png/                       # standalone PNG renders at standard sizes
    ├── qumesh-mark-16.png
    ├── qumesh-mark-32.png
    ├── …
    ├── qumesh-macos-128.png
    └── …
```

On macOS, the script uses the system `iconutil` for an optimized `.icns`. On Linux or Windows build hosts, it falls back to a Pillow-based ICNS writer that produces a functional but larger file.

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
