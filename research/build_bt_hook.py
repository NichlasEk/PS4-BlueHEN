#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build and host-test the 9.60 research hook. Never connects to a console."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

CAPTURE_SHA256 = '14ef08101c3716cc774ee2098a01b7278213d92768c7172e99b6309eebe31596'


def run(*args, timeout=30):
    return subprocess.check_output([str(x) for x in args], text=True, timeout=timeout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True, help='New output directory')
    parser.add_argument('--image', type=Path, help='Optional private RX capture for predicate tests')
    args = parser.parse_args()
    capture = args.image.resolve() if args.image else None
    if capture and hashlib.sha256(capture.read_bytes()).hexdigest() != CAPTURE_SHA256:
        parser.error('Unrecognized private capture; refuse to execute its instructions')
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).resolve().parent
    script = out / 'hook.ld'
    script.write_text('SECTIONS { . = 0; .text : { *(.text.entry) *(.text) '
                      '*(.text.trampolines) } . = 0x2000; .state : { *(.state) } '
                      'HIDDEN(bh_blob_end = .); /DISCARD/ : { *(.comment) '
                      '*(.note*) *(.eh_frame*) } }\n')
    run('cc', '-c', '-O2', '-Wall', '-Wextra', '-Werror', '-ffreestanding',
        '-fPIC', '-fno-builtin', '-fno-stack-protector', '-mno-red-zone',
        '-fno-asynchronous-unwind-tables', '-mgeneral-regs-only',
        source / 'bt-hook-control.c', '-o', out / 'control.o')
    for name in ('entry', 'trampolines'):
        run('cc', '-c', source / f'bt-hook-{name}.S', '-o', out / f'{name}.o')
    elf = out / 'hook.elf'
    run('ld', '-T', script, '-o', elf, out / 'entry.o', out / 'control.o', out / 'trampolines.o')
    if run('nm', '-u', elf).strip() or 'R_X86_64_' in run('readelf', '-rW', elf):
        raise RuntimeError('Unresolved symbols or runtime relocations')
    symbols = {name: int(value, 16) for value, _kind, name in
               (line.split() for line in run('nm', '-n', elf).splitlines())}
    if symbols['bh_blob_start'] != 0 or symbols['bh_state_data'] != 0x2000:
        raise RuntimeError('Unexpected resident layout')
    assembly = run('objdump', '-d', elf)
    (out / 'hook.asm').write_text(assembly)
    run('objcopy', '-O', 'binary', elf, out / 'hook.bin')
    data = (out / 'hook.bin').read_bytes()
    if len(data) != 0x20e0 or any(data[0x2000:]):
        raise RuntimeError('Unexpected initial state or blob size')
    run('cc', '-O2', '-Wall', '-Wextra', '-Werror', '-DBH_TRANSACTION_HOST_TEST',
        '-fno-pie', '-no-pie', '-pthread', source / 'test_bt_transaction_host.c',
        source / 'bt-hook-control.c', source / 'bt-hook-trampolines.S',
        '-o', out / 'test-transaction')
    transaction = run(out / 'test-transaction', timeout=5).strip()
    manifest = {'scope': 'host-tested; no console operation', 'bytes': len(data),
                'sha256': hashlib.sha256(data).hexdigest(), 'symbols': symbols,
                'transaction_tests': transaction, 'predicate_tests': None}
    if capture:
        run('cc', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-pie', '-no-pie',
            source / 'test_bt_hook_host.c', source / 'bt-hook-trampolines.S',
            '-o', out / 'test-hook')
        manifest['predicate_tests'] = run(out / 'test-hook', capture, timeout=5).strip()
        manifest['far_predicate_tests'] = run(
            out / 'test-hook', capture, out / 'hook.bin',
            symbols['bh_primary_trampoline'], symbols['bh_secondary_trampoline'],
            timeout=5).strip()
        # The original windows must each decode as ONE complete instruction.
        for address, size, mnemonic in ((0x5b2dad, 6, 'testb'), (0x59f966, 8, 'mov')):
            text = run('objdump', '-D', '-b', 'binary', '-m', 'i386:x86-64',
                       '--insn-width=16', f'--start-address={address}',
                       f'--stop-address={address+size}', capture)
            instructions = re.findall(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2}\s+)+([a-z][a-z0-9]*)', text, re.M)
            if instructions != [mnemonic]:
                raise RuntimeError(f'Patch window is not one complete {mnemonic} instruction')
        manifest['original_instruction_windows'] = ['0x5b2dad:6', '0x59f966:8']
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
