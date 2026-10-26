#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build local 9.60 entry/SMP probes. Does not connect to or modify a console."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def run(*args):
    return subprocess.check_output([str(arg) for arg in args], text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True,
                        help='New output directory; existing artifacts are never overwritten')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).resolve().parent
    script = out / 'probe.ld'
    script.write_text('SECTIONS { . = 0; .text : { *(.text.entry) *(.text) '
                      '*(.text.action) } /DISCARD/ : { *(.comment) *(.note*) '
                      '*(.eh_frame*) } }\n')
    run('cc', '-c', source / 'kernel-entry-probe.S', '-o', out / 'entry.o')
    run('cc', '-c', source / 'kernel-smp-probe.S', '-o', out / 'smp-asm.o')
    run('cc', '-c', '-O2', '-Wall', '-Wextra', '-Werror', '-ffreestanding',
        '-fPIC', '-fno-builtin', '-fno-stack-protector',
        '-fno-asynchronous-unwind-tables', '-mno-red-zone', '-mgeneral-regs-only',
        source / 'kernel-smp-probe.c', '-o', out / 'smp-c.o')
    manifest = {'scope': 'entry and temporary SMP observation only',
                'console_access': False, 'artifacts': {}}
    for name, objects in [('entry', ['entry.o']), ('smp', ['smp-asm.o', 'smp-c.o'])]:
        elf, binary = out / f'{name}.elf', out / f'{name}.bin'
        run('ld', '-T', script, '-o', elf, *(out / obj for obj in objects))
        if run('nm', '-u', elf).strip():
            raise RuntimeError('Unresolved symbols')
        relocations = run('readelf', '-rW', elf)
        if 'R_X86_64_' in relocations:
            raise RuntimeError('Runtime relocations are unsupported')
        (out / f'{name}.asm').write_text(run('objdump', '-d', elf))
        run('objcopy', '-O', 'binary', '-j', '.text', elf, binary)
        data = binary.read_bytes()
        if not 0 < len(data) < 0x1000:
            raise RuntimeError('Probe exceeds reserved code region')
        manifest['artifacts'][name] = {
            'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
