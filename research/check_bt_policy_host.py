#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check an offline 9.60 Bluetooth gate against a target-scoped prototype.

Runs selected original instructions in an isolated x86-64 child, with local
exit stubs. Requires a private capture; contains no firmware or console access.
"""
import argparse
import hashlib
import json
import platform
from pathlib import Path
import subprocess
import tempfile

PAGE_SHA256 = 'dc60d65592d86439f198b368144af147d7401397b978eb209e416a7f66dd8ca8'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path, help='Private kernel RX-segment capture')
    args = parser.parse_args()
    if platform.machine() != 'x86_64' or platform.system() != 'Linux':
        parser.error('Requires an x86-64 Linux host')
    image = args.image.resolve()
    with image.open('rb') as stream:
        if stream.read(6) != b'\x7fELF\x02\x01':
            parser.error('Expected ELF64 little-endian capture')
        stream.seek(0x5b2000)
        pages = stream.read(0x2000)
    if hashlib.sha256(pages).hexdigest() != PAGE_SHA256:
        parser.error('Code fingerprint mismatch; refuse to execute unknown code')
    with tempfile.TemporaryDirectory(prefix='bluehen-policy-') as tmp:
        executable = Path(tmp)/'check'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                        '-fno-pie', '-no-pie', str(Path(__file__).with_suffix('.c')),
                        '-o', str(executable)], check=True)
        result = subprocess.run([str(executable), str(image)], check=True,
                                capture_output=True, text=True, timeout=3)
    lines = result.stdout.splitlines()
    if len(lines) != 40:
        raise RuntimeError('Incomplete gate check results')
    print(json.dumps({'checks_passed': len(lines), 'code_sha256': PAGE_SHA256,
                      'results': lines, 'scope': 'offline admission gate only',
                      'console_modified': False}, indent=2))


if __name__ == '__main__':
    main()
