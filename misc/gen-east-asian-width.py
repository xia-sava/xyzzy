#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""East Asian Width が W か F の符号位置を、区間の表として書き出す。

入力は src/unicode/EastAsianWidth.txt、出力は src/east-asian-width.h。
xyzzy が升目に置くのは BMP の符号位置だけなので、BMP に絞る。

  python misc/gen-east-asian-width.py
"""

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
INPUT = REPO_ROOT / "src" / "unicode" / "EastAsianWidth.txt"
OUTPUT = REPO_ROOT / "src" / "east-asian-width.h"

BMP_LIMIT = 0x10000

# 割り当てのない符号位置も W を既定とする区画。入力の冒頭が挙げているもののうち、
# BMP にあるもの。割り当ての有無で穴を空けない
DEFAULT_WIDE = ((0x3400, 0x4DBF), (0x4E00, 0x9FFF), (0xF900, 0xFAFF))

VERSION_RE = re.compile(r"^#\s*EastAsianWidth-(\S+)\.txt")
ENTRY_RE = re.compile(r"^([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?\s*;\s*(\w+)")


def read_ucd():
    version = None
    codes = set()
    for line in INPUT.read_text(encoding="utf-8").splitlines():
        if version is None:
            m = VERSION_RE.match(line)
            if m:
                version = m.group(1)
        line = line.split("#", 1)[0]
        m = ENTRY_RE.match(line)
        if not m or m.group(3) not in ("W", "F"):
            continue
        lo = int(m.group(1), 16)
        hi = int(m.group(2), 16) if m.group(2) else lo
        codes.update(range(lo, min(hi, BMP_LIMIT - 1) + 1))
    if version is None:
        sys.exit("%s: 版が読み取れない" % INPUT)
    for lo, hi in DEFAULT_WIDE:
        codes.update(range(lo, hi + 1))
    return version, codes


def to_ranges(codes):
    ranges = []
    for cp in sorted(codes):
        if ranges and ranges[-1][1] + 1 == cp:
            ranges[-1][1] = cp
        else:
            ranges.append([cp, cp])
    return ranges


def main():
    version, codes = read_ucd()
    lines = [
        "/* East Asian Width が W か F の符号位置。二桁の升目に置く字にあたる。",
        "   misc/gen-east-asian-width.py が src/unicode/EastAsianWidth.txt",
        "   (Unicode %s) から作る。手で直さない */" % version,
        "",
        "static const struct",
        "{",
        "  ucs2_t from, to;",
        "} east_asian_wide_range[] =",
        "{",
    ]
    lines += ["  {0x%04x, 0x%04x}," % (lo, hi) for lo, hi in to_ranges(codes)]
    lines += ["};", ""]
    OUTPUT.write_text("\n".join(lines), encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
