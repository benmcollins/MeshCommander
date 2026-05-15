#!/usr/bin/env python3
"""
QuMesh icon build script.

Generates platform-specific icon bundles from the master SVG files:
  - macOS:   qumesh.icns (from svg/qumesh-macos.svg + svg/qumesh-mark-small.svg)
  - Windows: qumesh.ico  (from svg/qumesh-windows.svg + svg/qumesh-mark-small.svg)
  - PNG renders: build/png/ at every standard size
  - Social preview: build/og-image.png (1280×640, from svg/qumesh-og.svg) —
    upload at Repo → Settings → Social preview.

Requirements:
  pip install cairosvg pillow

On macOS, iconutil (built-in) is used to assemble the .icns from an iconset.
On other platforms, the .icns is assembled using the Pillow-based approach.

Usage:
  python build-icons.py
"""

from __future__ import annotations

import io
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

try:
    import cairosvg
    from PIL import Image
except ImportError:
    print("Missing dependencies. Install with: pip install cairosvg pillow", file=sys.stderr)
    sys.exit(1)


ROOT = Path(__file__).parent
SVG_DIR = ROOT / "svg"
BUILD_DIR = ROOT / "build"
PNG_DIR = BUILD_DIR / "png"
ICONSET_DIR = BUILD_DIR / "qumesh.iconset"

# Source SVGs.
MACOS_SVG = SVG_DIR / "qumesh-macos.svg"
WINDOWS_SVG = SVG_DIR / "qumesh-windows.svg"
SMALL_SVG = SVG_DIR / "qumesh-mark-small.svg"
OG_SVG = SVG_DIR / "qumesh-og.svg"

# macOS iconset sizes: (size_px, filename, source_svg).
# Apple expects @1x and @2x variants. We use the simplified mark for sizes <= 32px
# because the lattice detail blurs out and becomes visual noise.
MACOS_SIZES = [
    (16,   "icon_16x16.png",       SMALL_SVG),
    (32,   "icon_16x16@2x.png",    SMALL_SVG),
    (32,   "icon_32x32.png",       SMALL_SVG),
    (64,   "icon_32x32@2x.png",    MACOS_SVG),
    (128,  "icon_128x128.png",     MACOS_SVG),
    (256,  "icon_128x128@2x.png",  MACOS_SVG),
    (256,  "icon_256x256.png",     MACOS_SVG),
    (512,  "icon_256x256@2x.png",  MACOS_SVG),
    (512,  "icon_512x512.png",     MACOS_SVG),
    (1024, "icon_512x512@2x.png",  MACOS_SVG),
]

# Windows .ico sizes. The standard set per Microsoft's guidelines.
WINDOWS_SIZES = [
    (16,  SMALL_SVG),
    (20,  SMALL_SVG),
    (24,  SMALL_SVG),
    (32,  SMALL_SVG),
    (40,  WINDOWS_SVG),
    (48,  WINDOWS_SVG),
    (64,  WINDOWS_SVG),
    (96,  WINDOWS_SVG),
    (128, WINDOWS_SVG),
    (256, WINDOWS_SVG),
]


def render_svg_to_png(svg_path: Path, size: int) -> bytes:
    """Render an SVG to a PNG at the given size (square) and return PNG bytes."""
    return cairosvg.svg2png(
        url=str(svg_path),
        output_width=size,
        output_height=size,
    )


def build_macos_iconset() -> None:
    """Build the macOS .iconset directory and assemble it into a .icns."""
    if ICONSET_DIR.exists():
        shutil.rmtree(ICONSET_DIR)
    ICONSET_DIR.mkdir(parents=True)

    print("Building macOS iconset…")
    for size, filename, source in MACOS_SIZES:
        png_bytes = render_svg_to_png(source, size)
        out_path = ICONSET_DIR / filename
        out_path.write_bytes(png_bytes)
        print(f"  {filename} ({size}px from {source.name})")

    icns_out = BUILD_DIR / "qumesh.icns"

    # Prefer iconutil on macOS; it produces a smaller, optimized .icns.
    if shutil.which("iconutil"):
        subprocess.run(
            ["iconutil", "-c", "icns", str(ICONSET_DIR), "-o", str(icns_out)],
            check=True,
        )
        print(f"  -> {icns_out} (via iconutil)")
    else:
        # Fallback: assemble .icns manually using Pillow.
        # This is a simplified ICNS writer supporting the common icon types.
        _write_icns_with_pillow(icns_out)
        print(f"  -> {icns_out} (via Pillow fallback)")


# ICNS type codes for PNG-encoded icons.
ICNS_TYPES = {
    16:   b"icp4",  # 16x16
    32:   b"icp5",  # 32x32
    64:   b"icp6",  # 64x64
    128:  b"ic07",  # 128x128
    256:  b"ic08",  # 256x256
    512:  b"ic09",  # 512x512
    1024: b"ic10",  # 1024x1024 (also serves as 512@2x)
}


def _write_icns_with_pillow(out_path: Path) -> None:
    """Fallback .icns writer for non-macOS build hosts."""
    entries: list[tuple[bytes, bytes]] = []
    for size, type_code in ICNS_TYPES.items():
        # Use macos.svg for >= 64px, small.svg for < 64px.
        source = MACOS_SVG if size >= 64 else SMALL_SVG
        png_bytes = render_svg_to_png(source, size)
        entries.append((type_code, png_bytes))

    # ICNS format: 'icns' magic + total size (uint32 BE), then a sequence of
    # 4-byte type codes + 4-byte length (including header) + payload.
    body = b""
    for type_code, png in entries:
        body += type_code + struct.pack(">I", len(png) + 8) + png
    icns = b"icns" + struct.pack(">I", len(body) + 8) + body
    out_path.write_bytes(icns)


def build_windows_ico() -> None:
    """Build the multi-resolution Windows .ico file."""
    print("Building Windows .ico…")
    images: list[Image.Image] = []
    sizes_for_ico: list[tuple[int, int]] = []
    for size, source in WINDOWS_SIZES:
        png_bytes = render_svg_to_png(source, size)
        img = Image.open(io.BytesIO(png_bytes)).convert("RGBA")
        images.append(img)
        sizes_for_ico.append((size, size))
        print(f"  {size}x{size} from {source.name}")

    ico_out = BUILD_DIR / "qumesh.ico"
    # Pillow writes a multi-image .ico by passing all sizes to the largest image's save().
    images[-1].save(
        ico_out,
        format="ICO",
        sizes=sizes_for_ico,
        append_images=images[:-1],
    )
    print(f"  -> {ico_out}")


def build_png_renders() -> None:
    """Emit standalone PNG renders at every size for general use (Linux, web, docs)."""
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    print("Building PNG renders…")

    standard_sizes = [16, 24, 32, 48, 64, 96, 128, 256, 512, 1024]
    for size in standard_sizes:
        # Master mark on transparent.
        mark_source = SMALL_SVG if size <= 32 else SVG_DIR / "qumesh-mark.svg"
        # Render the mark in slate for general use.
        # We embed the slate color into a temporary copy by replacing currentColor.
        svg_text = mark_source.read_text().replace("currentColor", "#1e293b")
        png_bytes = cairosvg.svg2png(
            bytestring=svg_text.encode(),
            output_width=size,
            output_height=size,
        )
        out_path = PNG_DIR / f"qumesh-mark-{size}.png"
        out_path.write_bytes(png_bytes)

        # macOS squircle tile.
        if size >= 64:
            png_bytes = render_svg_to_png(MACOS_SVG, size)
            (PNG_DIR / f"qumesh-macos-{size}.png").write_bytes(png_bytes)

    print(f"  -> {PNG_DIR}/")


def build_og_image() -> None:
    """Render the 1280×640 social preview PNG used as GitHub's Open
    Graph image. Best results need IBM Plex Sans available to
    fontconfig; the bundled TTFs live at app/qml/fonts/. The SVG
    falls back to Helvetica Neue / Arial if Plex isn't installed."""
    print("Building social preview image…")
    out_path = BUILD_DIR / "og-image.png"
    png_bytes = cairosvg.svg2png(
        url=str(OG_SVG),
        output_width=1280,
        output_height=640,
    )
    out_path.write_bytes(png_bytes)
    print(f"  -> {out_path}")


def main() -> int:
    if not SVG_DIR.is_dir():
        print(f"SVG source directory not found: {SVG_DIR}", file=sys.stderr)
        return 1

    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    build_png_renders()
    build_macos_iconset()
    build_windows_ico()
    build_og_image()

    print("\nDone. Outputs in build/:")
    for p in sorted(BUILD_DIR.rglob("*")):
        if p.is_file():
            print(f"  {p.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
