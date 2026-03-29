# -*- coding: utf-8 -*-
"""Gỡ // và /* */ trong file .ino/.cpp (tôn trọng chuỗi trong \" và ')."""
from __future__ import annotations

import pathlib
import re
import sys
from typing import List, Optional


def strip_cpp_comments(text: str) -> str:
    out: List[str] = []
    i = 0
    n = len(text)
    in_string: Optional[str] = None
    in_block = False
    in_line = False

    while i < n:
        if in_line:
            if text[i] == "\n":
                in_line = False
                out.append("\n")
            i += 1
            continue
        if in_block:
            if text[i] == "*" and i + 1 < n and text[i + 1] == "/":
                in_block = False
                i += 2
            else:
                i += 1
            continue
        if in_string:
            out.append(text[i])
            if text[i] == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if text[i] == in_string:
                in_string = None
            i += 1
            continue

        if text[i] == "/" and i + 1 < n:
            if text[i + 1] == "/":
                in_line = True
                i += 2
                continue
            if text[i + 1] == "*":
                in_block = True
                i += 2
                continue
        if text[i] in "\"'":
            in_string = text[i]
            out.append(text[i])
            i += 1
            continue
        out.append(text[i])
        i += 1

    s = "".join(out)
    s = re.sub(r"\n{3,}", "\n\n", s)
    return s.strip() + "\n"


def main() -> None:
    root = pathlib.Path(__file__).resolve().parent.parent
    src = root / "iot-nhom6" / "iot-nhom6.ino"
    dst = root / "appendix-iot-nhom6-nocomment.ino"
    if len(sys.argv) >= 2:
        src = pathlib.Path(sys.argv[1])
    if len(sys.argv) >= 3:
        dst = pathlib.Path(sys.argv[2])
    text = src.read_text(encoding="utf-8", errors="replace")
    cleaned = strip_cpp_comments(text)
    dst.write_text(cleaned, encoding="utf-8")
    print(f"Wrote {dst} ({len(cleaned.splitlines())} lines)")


if __name__ == "__main__":
    main()
