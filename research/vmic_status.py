#!/usr/bin/env python3
"""Read-only 9.60 Vmic metadata snapshot. Never reads PCM or invokes AudioIn.

Requires the existing local ps4remote/probe debug_client.py via --client-dir.
That transport must be trusted; this script uses only its metadata/read methods.
Offsets apply only after firmware, process identity and code-hash checks pass.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import struct
import sys

CODE_HASHES = {
    0x28840: '753db441f8167c4ae7e1f9fa27f3d7a7b4de8d27f58ee8873bbe1ad6d4fbcc66',
    0x28960: '4c94d117a5be86ccf5724a0172b9676a44b943af6edc44056e4a7afcfc8346ca',
    0x28700: '5eb685d136ee5b85e64645d70f032a48274367b8eacdcc155aa19ea48b94f2d6',
    0x29210: '9cb3b34f3ed977a2e6ddc0d7d19e99c5d4faedb2d5640eedefd1a5093615820a',
}
STRIDE = 0x2034
SLOTS = 4


def snapshot(client):
    version = client.version()
    if version['firmware_raw'] != 960 or version['platform'] != 4:
        raise ValueError('Requires PS4 firmware 9.60')
    processes = [p for p in client.processes() if p['name'] == 'orbis_audiod.elf']
    if len(processes) != 1:
        raise ValueError('Expected exactly one audio daemon')
    pid = processes[0]['pid']
    identity = client.info(pid)
    if (identity['path'], identity['title_id']) != (
            '/system/sys/orbis_audiod.elf', 'NPXS20973'):
        raise ValueError('Audio daemon identity mismatch')
    maps = client.maps(pid)
    executable = [m for m in maps if m['name'] == 'executable' and m['prot'] & 5 == 5]
    if len(executable) != 1:
        raise ValueError('Ambiguous executable mapping')
    base = executable[0]['start']

    def read(address, length):
        if not any(m['prot'] & 1 and m['start'] <= address and
                   address + length <= m['end'] for m in maps):
            raise ValueError('Read outside known readable mappings')
        data = client.read(pid, address, length)
        if len(data) != length:
            raise ValueError('Short debugger read')
        return data

    for offset, expected in CODE_HASHES.items():
        if hashlib.sha256(read(base + offset, 32)).hexdigest() != expected:
            raise ValueError(f'Firmware code mismatch at {offset:#x}')
    pointer_bytes = read(base + 0x2c0410, 8)
    pointer, = struct.unpack('<Q', pointer_bytes)
    initialized = read(base + 0x2c0418, 1)[0]
    if initialized != 1 or not pointer:
        raise ValueError('Vmic shared metadata is not initialized')
    if not any(m['name'] == '/vmicDdShmAin' and m['prot'] & 1 and
               m['start'] <= pointer and pointer + SLOTS * STRIDE <= m['end']
               for m in maps):
        raise ValueError('Vmic pointer does not fit the named shared mapping')

    def slots():
        result = []
        for slot in range(SLOTS):
            address = pointer + slot * STRIDE
            active, owner_pid = struct.unpack('<II', read(address, 8))
            # Skip the PCM buffers and producer/consumer synchronization fields.
            channels, rate, frames = struct.unpack('<III', read(address + 0x2018, 12))
            handle, = struct.unpack('<I', read(address + 0x2030, 4))
            if active not in (0, 1):
                raise ValueError('Unexpected slot state')
            if active and (not owner_pid or handle & 0xffff00ff != 0x40000 + slot):
                raise ValueError('Unexpected active handle or owner')
            result.append(dict(slot=slot, active=bool(active), owner_pid=owner_pid,
                               handle=handle, channels=channels,
                               sample_rate=rate, frames=frames))
        return result

    first = slots()
    second = slots()
    if first != second:
        raise ValueError('Slot metadata changed during observation; no stable snapshot')
    if (read(base + 0x2c0410, 8) != pointer_bytes or
            read(base + 0x2c0418, 1) != b'\x01' or client.info(pid) != identity):
        raise ValueError('Daemon state changed during observation')
    return dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                version=version, identity=identity, code_checks=len(CODE_HASHES),
                observation='two matching reads; not an atomic snapshot',
                pcm_read=False, mutations=False, slots=second)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host')
    parser.add_argument('--client-dir', type=Path, required=True)
    args = parser.parse_args()
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(args.client_dir.resolve()))
    from debug_client import DebugClient
    client = DebugClient(args.host, port=744, timeout=5)
    try:
        print(json.dumps(snapshot(client), indent=2))
    finally:
        client.close()


if __name__ == '__main__':
    main()
