#!/usr/bin/env python3
"""Offline Orbis ELF export metadata; no code execution or raw-byte output.

Input must be an ELF extracted from the user's console, not an encrypted SELF.
Outputs only its digest, requested symbol matches, offsets, sizes and code hashes.
Names are matched by Orbis NID; matching a name does not recover its ABI.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import struct

DEFAULT_NAMES = [
    'sceBluetoothHidGetDeviceInfo', 'sceBluetoothHidInit',
    'sceAudioInOpen', 'sceAudioInInput', 'sceAudioInSetConnections',
    'sceAudioInVmicCreate', 'sceAudioInVmicWrite', 'sceAudioInVmicDestroy',
]


def nid(name):
    salt = bytes.fromhex('518D64A635DED8C1E6B039B1C3E55230')
    digest = hashlib.sha1(name.encode('ascii') + salt).digest()[:8][::-1]
    return base64.b64encode(digest).decode().rstrip('=').replace('/', '-')


def inspect(data, names):
    def span(offset, size):
        if offset < 0 or size < 0 or offset + size > len(data):
            raise ValueError('ELF range outside input')
        return data[offset:offset + size]

    def unpack(fmt, offset):
        return struct.unpack(fmt, span(offset, struct.calcsize(fmt)))

    if span(0, 6) != b'\x7fELF\x02\x01':
        raise ValueError('Expected little-endian ELF64, not SELF')
    if unpack('<H', 18)[0] != 62:
        raise ValueError('Expected x86-64 ELF')
    phoff = unpack('<Q', 32)[0]
    phsize, phcount = unpack('<HH', 54)
    if phsize != 56 or not 0 < phcount <= 128:
        raise ValueError('Unsupported program header layout')
    headers = [unpack('<IIQQQQQQ', phoff + i * phsize) for i in range(phcount)]
    for h in headers:
        span(h[2], h[5])
    dynlib = [h for h in headers if h[0] == 0x61000000]
    dynamic = [h for h in headers if h[0] == 2]
    if len(dynlib) != 1 or len(dynamic) != 1:
        raise ValueError('Expected one Orbis dynlib-data and dynamic segment')
    base, base_size = dynlib[0][2], dynlib[0][5]
    dyn = dynamic[0]
    if dyn[5] % 16:
        raise ValueError('Truncated dynamic table')
    tags = {}
    for off in range(dyn[2], dyn[2] + dyn[5], 16):
        key, value = unpack('<QQ', off)
        if key == 0:
            break
        tags[key] = value

    def table(offset_tag, size_tag):
        if offset_tag not in tags or size_tag not in tags:
            raise ValueError('Missing Orbis table tags')
        offset, size = tags[offset_tag], tags[size_tag]
        if offset + size > base_size:
            raise ValueError('Table outside dynlib-data segment')
        return span(base + offset, size)

    strings = table(0x61000035, 0x61000037)
    symbols = table(0x61000039, 0x6100003f)
    if len(symbols) % 24:
        raise ValueError('Truncated symbol table')
    wanted = {nid(name): name for name in names}
    matches = []
    for off in range(0, len(symbols), 24):
        nameoff, info, other, section, value, size = struct.unpack_from('<IBBHQQ', symbols, off)
        end = strings.find(b'\0', nameoff)
        if nameoff >= len(strings) or end < 0:
            raise ValueError('Invalid symbol string')
        identifier = strings[nameoff:end].decode('ascii').split('#')[0]
        if not section or identifier not in wanted:
            continue
        record = {'name': wanted[identifier], 'nid': identifier,
                  'virtual_offset': value, 'symbol_size': size}
        for h in headers:
            if h[0] == 1 and h[1] & 1 and h[3] <= value and value + 32 <= h[3] + h[5]:
                fileoff = h[2] + value - h[3]
                record.update(file_offset=fileoff, prefix_bytes=32,
                              prefix_sha256=hashlib.sha256(span(fileoff, 32)).hexdigest())
                break
        matches.append(record)
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'exports': matches,
            'unmatched': sorted(set(names) - {r['name'] for r in matches})}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    parser.add_argument('--symbol', action='append', dest='names')
    args = parser.parse_args()
    try:
        result = inspect(args.elf.read_bytes(), args.names or DEFAULT_NAMES)
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f'{error}\n')
    print(json.dumps(result, indent=2))
