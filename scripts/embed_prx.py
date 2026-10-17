#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Embed a .prx as a C byte array header.

Usage: embed_prx.py <prx> <header.h> [symbol_name]
Default symbol: shell_audio_tap_prx
"""
import sys
from pathlib import Path

src, dst = Path(sys.argv[1]), Path(sys.argv[2])
sym = sys.argv[3] if len(sys.argv) > 3 else "shell_audio_tap_prx"
data = src.read_bytes()
lines = [
    "/* Auto-generated from %s - do not edit. */" % src.name,
    "unsigned char %s[] = {" % sym,
]
for i in range(0, len(data), 12):
    chunk = data[i : i + 12]
    lines.append("  " + ", ".join("0x%02x" % b for b in chunk) + ",")
lines.append("};")
lines.append("unsigned int %s_len = %d;" % (sym, len(data)))
dst.write_text("\n".join(lines) + "\n")
print("wrote %s (%d bytes, symbol %s)" % (dst, len(data), sym))
