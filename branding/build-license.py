#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

"""Convert LICENSE.md → LICENSE.rtf for the macOS DMG and Windows NSIS
license-acceptance dialogs (and for the copy bundled into the app
itself).

The NSIS license dialog and macOS hdiutil's `SLAResources` both accept
RTF or plain text — not Markdown — so without conversion the dialogs
show literal `#`, `**`, `_` punctuation. RTF lets us preserve the
heading hierarchy, bold/italic emphasis, and bullet structure of the
Apache 2.0 boilerplate.

Why not pandoc: the GitHub-hosted runners don't ship it, and we don't
want a build dep that's "usually fine, sometimes 502s on a fresh CI VM"
for a one-shot text conversion of a static file. The Markdown subset
used by `LICENSE.md` is small enough to handle directly with the
stdlib.

Supported Markdown subset (just what `LICENSE.md` and the bundled
third-party notice section actually use):

  - `# Title`, `## Heading`, `#### Sub-heading`
  - `**bold**`
  - `_italic_`
  - inline `` `code` ``  (rendered in a fixed-width font)
  - `<URL>` autolinks (rendered as plain URL text)
  - `*  bullet` lists
  - blank-line paragraph breaks
  - trailing two-space line breaks within a paragraph
  - HTML entities `&lt; &gt; &amp; &quot;`
  - non-ASCII code points (emitted as RTF `\\u<int>?` escapes)

Usage:
  build-license.py <input.md> <output.rtf>
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


# Font / size table. RTF font sizes are half-points (e.g. 24 = 12pt).
FONT_SANS = 0
FONT_SANS_BOLD = 1
FONT_SANS_ITALIC = 2
FONT_MONO = 3

H1_SIZE = 36   # 18pt
H2_SIZE = 32   # 16pt
H3_SIZE = 28   # 14pt
H4_SIZE = 26   # 13pt
BODY_SIZE = 22 # 11pt — readable in a small dialog, fits a lot of text


# ---- escape helpers ---------------------------------------------------

_HTML_ENTITY = re.compile(r"&(lt|gt|amp|quot|nbsp);")
_HTML_DECODE = {
    "lt": "<",
    "gt": ">",
    "amp": "&",
    "quot": '"',
    "nbsp": " ",
}


def _html_decode(text: str) -> str:
    return _HTML_ENTITY.sub(lambda m: _HTML_DECODE[m.group(1)], text)


def _rtf_escape(text: str) -> str:
    """Escape a run of plain text for RTF.

    RTF special chars: backslash, braces, and any non-ASCII code point.
    Non-ASCII gets the `\\uN?` form where N is the signed 16-bit code
    unit and `?` is the ASCII fallback. UTF-16 surrogates for codepoints
    above the BMP are emitted as two consecutive `\\u` escapes.
    """
    out: list[str] = []
    for ch in text:
        cp = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == "{":
            out.append("\\{")
        elif ch == "}":
            out.append("\\}")
        elif cp < 128:
            out.append(ch)
        elif cp <= 0xFFFF:
            # \uN must be a signed 16-bit int. Wrap values > 32767.
            n = cp if cp < 0x8000 else cp - 0x10000
            out.append(f"\\u{n}?")
        else:
            # BMP-outside char: encode as UTF-16 surrogate pair.
            cp -= 0x10000
            hi = 0xD800 + (cp >> 10)
            lo = 0xDC00 + (cp & 0x3FF)
            for n16 in (hi, lo):
                n = n16 if n16 < 0x8000 else n16 - 0x10000
                out.append(f"\\u{n}?")
    return "".join(out)


# ---- inline span parser -----------------------------------------------
#
# A tiny state machine over the line: walk characters, emit RTF as we
# open and close `**…**`, `_…_`, `` `…` ``, and `<URL>` runs. We
# intentionally do not try to be a full CommonMark parser — the
# LICENSE source is hand-authored and only uses the constructs above.

_AUTOLINK = re.compile(r"<(https?://[^>\s]+)>")


def _render_inline(text: str) -> str:
    text = _html_decode(text)
    # Autolinks: <http://…> → the URL only, as plain text. The license
    # dialog isn't a browser, so a literal underlined URL would imply
    # interactivity we don't actually wire up.
    text = _AUTOLINK.sub(lambda m: m.group(1), text)

    out: list[str] = []
    i = 0
    n = len(text)
    bold = False
    italic = False
    mono = False

    def open_span(token: str) -> None:
        out.append(token)

    def close_span(token: str) -> None:
        out.append(token)

    while i < n:
        ch = text[i]
        # **bold**
        if ch == "*" and i + 1 < n and text[i + 1] == "*":
            if bold:
                out.append("}")
                bold = False
            else:
                out.append("{\\b ")
                bold = True
            i += 2
            continue
        # _italic_  — require non-alnum context on the opening edge so we
        # don't trip on identifiers like `snake_case`. LICENSE.md only
        # uses underscores for whole-phrase emphasis, and treats the
        # phrase boundaries as punctuation or whitespace, so the rule is:
        # open if the prev char is start-of-line/whitespace/punctuation
        # and the next char is non-whitespace; close if the prev char is
        # non-whitespace and the next char is end-of-line/whitespace/
        # punctuation.
        if ch == "_":
            prev_ch = text[i - 1] if i > 0 else ""
            next_ch = text[i + 1] if i + 1 < n else ""
            prev_is_alnum = prev_ch.isalnum() or prev_ch == "_"
            next_is_alnum = next_ch.isalnum() or next_ch == "_"
            if italic:
                # close: prev must be non-whitespace, next must not be alnum
                ok = (prev_ch and not prev_ch.isspace()) and not next_is_alnum
                if ok:
                    out.append("}")
                    italic = False
                    i += 1
                    continue
            else:
                # open: prev must not be alnum, next must be non-whitespace
                ok = (not prev_is_alnum) and (next_ch and not next_ch.isspace())
                if ok:
                    out.append("{\\i ")
                    italic = True
                    i += 1
                    continue
        # `code`
        if ch == "`":
            if mono:
                out.append("}")
                mono = False
            else:
                out.append("{\\f3 ")
                mono = True
            i += 1
            continue
        out.append(_rtf_escape(ch))
        i += 1

    # Defensive close of any still-open spans (malformed input).
    if mono:
        out.append("}")
    if italic:
        out.append("}")
    if bold:
        out.append("}")
    return "".join(out)


# ---- block-level walker -----------------------------------------------


def _convert(md: str) -> str:
    lines = md.splitlines()
    body: list[str] = []
    paragraph: list[str] = []

    def flush_paragraph() -> None:
        if not paragraph:
            return
        # Trailing-two-space → line break, otherwise join with spaces.
        rendered: list[str] = []
        for idx, line in enumerate(paragraph):
            if line.endswith("  "):
                rendered.append(_render_inline(line.rstrip()) + "\\line ")
            else:
                rendered.append(_render_inline(line))
            if idx < len(paragraph) - 1 and not line.endswith("  "):
                rendered.append(" ")
        body.append("\\pard\\sa120\\sl276\\slmult1 " + "".join(rendered) + "\\par\n")
        paragraph.clear()

    def emit_heading(level: int, text: str) -> None:
        flush_paragraph()
        size = {1: H1_SIZE, 2: H2_SIZE, 3: H3_SIZE, 4: H4_SIZE}.get(level, H4_SIZE)
        # Space-before for h2+ to separate sections; tighter line spacing.
        sb = 240 if level >= 2 else 120
        body.append(
            f"\\pard\\sb{sb}\\sa120\\keepn\\f1\\fs{size}\\b "
            + _render_inline(text)
            + "\\b0\\f0\\fs"
            + str(BODY_SIZE)
            + "\\par\n"
        )

    def emit_bullet(text: str) -> None:
        # Use a literal bullet glyph (U+2022) with a hanging indent.
        body.append(
            "\\pard\\fi-360\\li720\\sa80\\sl276\\slmult1 "
            + "\\u8226?\\tab "
            + _render_inline(text)
            + "\\par\n"
        )

    setext_h1 = re.compile(r"^=+\s*$")
    setext_h2 = re.compile(r"^-+\s*$")
    atx = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
    bullet = re.compile(r"^\s*[*\-]\s+(.*)$")
    indented_continuation = re.compile(r"^(?:    |\t| {2,})(\S.*)$")

    i = 0
    while i < len(lines):
        raw = lines[i]
        line = raw.rstrip("\r")

        # ATX heading: `## Title`
        m = atx.match(line)
        if m:
            emit_heading(len(m.group(1)), m.group(2))
            i += 1
            continue

        # Setext heading: a non-blank line followed by `===` or `---`.
        if i + 1 < len(lines) and line.strip() and not paragraph:
            nxt = lines[i + 1].rstrip("\r")
            if setext_h1.match(nxt):
                emit_heading(1, line)
                i += 2
                continue
            if setext_h2.match(nxt):
                emit_heading(2, line)
                i += 2
                continue

        # Bullet, plus any continuation lines that follow it without an
        # intervening blank. This covers both indented continuations
        # (the SIL/JetBrains notes near the bottom of LICENSE.md) and
        # CommonMark's "lazy" continuation where the wrapped line sits
        # flush-left (the Apache § 4 (a)/(b)/(c)/(d) bullets).
        m = bullet.match(line)
        if m:
            flush_paragraph()
            parts = [m.group(1)]
            j = i + 1
            while j < len(lines):
                cont = lines[j].rstrip("\r")
                if not cont.strip():
                    break  # blank ends the bullet
                if atx.match(cont) or bullet.match(cont):
                    break  # next block starts
                cm = indented_continuation.match(cont)
                if cm:
                    parts.append(cm.group(1))
                else:
                    parts.append(cont)
                j += 1
            emit_bullet(" ".join(parts))
            i = j
            continue

        # Blank line: flushes the current paragraph.
        if not line.strip():
            flush_paragraph()
            i += 1
            continue

        # Plain paragraph text.
        paragraph.append(line)
        i += 1

    flush_paragraph()
    return "".join(body)


# ---- RTF preamble -----------------------------------------------------


_PREAMBLE = (
    "{\\rtf1\\ansi\\ansicpg1252\\cocoartf2761\n"
    "{\\fonttbl"
    "\\f0\\fswiss\\fcharset0 Helvetica;"
    "\\f1\\fswiss\\fcharset0 Helvetica-Bold;"
    "\\f2\\fswiss\\fcharset0 Helvetica-Oblique;"
    "\\f3\\fmodern\\fcharset0 Menlo-Regular;"
    "}\n"
    "{\\colortbl;\\red0\\green0\\blue0;\\red80\\green80\\blue80;}\n"
    "\\margl1080\\margr1080\\margt1080\\margb1080\n"
    "\\vieww12000\\viewh15000\\viewkind0\n"
    "\\f0\\fs" + str(BODY_SIZE) + "\n"
)
_POSTAMBLE = "}\n"


def md_to_rtf(md: str) -> str:
    return _PREAMBLE + _convert(md) + _POSTAMBLE


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(f"usage: {argv[0]} <input.md> <output.rtf>", file=sys.stderr)
        return 2
    src = Path(argv[1])
    dst = Path(argv[2])
    md = src.read_text(encoding="utf-8")
    rtf = md_to_rtf(md)
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(rtf, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
