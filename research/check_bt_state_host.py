#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Execute the fingerprinted private 9.60 state query with synthetic host data."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
image = Path(sys.argv[1]).resolve()
if hashlib.sha256(image.read_bytes()).hexdigest() != '14ef08101c3716cc774ee2098a01b7278213d92768c7172e99b6309eebe31596':
    raise SystemExit('Unrecognized capture; refusing execution')
with tempfile.TemporaryDirectory(prefix='bluehen-state-') as tmp:
    exe = Path(tmp) / 'check'
    subprocess.run(['cc', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-pie', '-no-pie',
                    str(Path(__file__).with_suffix('.c')), '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(image)], check=True, timeout=5)
