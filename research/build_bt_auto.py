#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a separate native automatic hook; never touches accepted manual artifacts."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

def run(*args):
    return subprocess.check_output([str(a) for a in args], text=True, timeout=30)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--image', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a=p.parse_args();data=a.image.read_bytes()
    if hashlib.sha256(data).hexdigest()!='14ef08101c3716cc774ee2098a01b7278213d92768c7172e99b6309eebe31596':
        p.error('Unrecognized private capture')
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=False);src=Path(__file__).resolve().parent
    ranges=((0x1be4e0,0x70),(0x15a6d,32),(0x52240,0x310),(0x457c50,64),(0x1889d0,0x260),(0x5b2000,8192),(0x59f000,4096))
    rows=[]
    for off,n in ranges:
        digest=hashlib.sha256(data[off:off+n]).digest()
        rows.append('{'+f'{off},{n},'+'{'+','.join(str(x) for x in digest)+'}}')
    (out/'bt-auto-fingerprints.h').write_text('struct bh_fingerprint {unsigned offset,size;unsigned char sha[32];};\nstatic const struct bh_fingerprint bh_fingerprints_960[]={'+','.join(rows)+'};\n')
    common=['-DBH_AUTOMATIC','-O2','-Wall','-Wextra','-Werror']
    run('cc',*common,'-c','-ffreestanding','-fPIC','-fno-builtin','-fno-stack-protector','-mno-red-zone','-fno-asynchronous-unwind-tables','-mgeneral-regs-only','-I',out,src/'bt-auto-control.c','-o',out/'control.o')
    for name in ('entry','trampolines'):
        run('cc','-DBH_AUTOMATIC','-c',src/f'bt-hook-{name}.S','-o',out/f'{name}.o')
    (out/'hook.ld').write_text('SECTIONS { . = 0; .text : { *(.text.entry) *(.text) *(.text.trampolines) *(.rodata*) } . = 0x3000; .state : { *(.state) } HIDDEN(bh_blob_end = .); /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) } }')
    elf=out/'hook.elf';run('ld','-T',out/'hook.ld','-o',elf,out/'entry.o',out/'control.o',out/'trampolines.o')
    assert not run('nm','-u',elf).strip() and 'R_X86_64_' not in run('readelf','-rW',elf)
    symbols={name:int(value,16) for value,kind,name in (line.split() for line in run('nm','-n',elf).splitlines())}
    assert symbols['bh_blob_start']==0 and symbols['bh_state_data']==0x3000
    run('objcopy','-O','binary',elf,out/'hook.bin');blob=(out/'hook.bin').read_bytes();assert len(blob)==0x30f0 and not any(blob[0x3000:])
    (out/'hook.asm').write_text(run('objdump','-d',elf))
    run('cc',*common,'-fno-pie','-no-pie',src/'test_bt_hook_host.c',src/'bt-hook-trampolines.S','-o',out/'test-hook')
    checks=run(out/'test-hook',a.image.resolve())
    far=run(out/'test-hook',a.image.resolve(),out/'hook.bin',symbols['bh_primary_trampoline'],symbols['bh_secondary_trampoline'])
    run('cc',*common,'-DBH_TRANSACTION_HOST_TEST','-fno-pie','-no-pie','-pthread',src/'test_bt_transaction_host.c',src/'bt-hook-control.c',src/'bt-hook-trampolines.S','-o',out/'test-transaction')
    transaction=run(out/'test-transaction')
    run('cc',*common,'-fno-pie','-no-pie','-I',out,src/'test_bt_auto_integrity.c',src/'bt-hook-trampolines.S','-o',out/'test-integrity')
    integrity=run(out/'test-integrity',a.image.resolve())
    (out/'bt-auto-blob.h').write_text('static const unsigned char bh_auto_blob[] __attribute__((aligned(16384)))={'+','.join(str(x) for x in blob)+'};\n')
    manifest=dict(bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),symbols=symbols,checks=checks,far_checks=far,transactions=transaction,integrity=integrity,scope='host only; no console operation')
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))
if __name__=='__main__':main()
