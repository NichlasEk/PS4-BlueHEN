#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Embed src/web/index.html as a C string: embed_html.py <in.html> <out.h>"""
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_text(encoding="utf-8")
out = ["/* Auto-generated from %s - do not edit. */" % Path(sys.argv[1]).name,
       "static const char webui_html[] ="]
for line in data.splitlines():
    s = line.replace("\\", "\\\\").replace('"', '\\"')
    out.append('  "%s\\n"' % s)
out.append(";")
Path(sys.argv[2]).write_text("\n".join(out) + "\n", encoding="utf-8")
