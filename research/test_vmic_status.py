import hashlib
import struct
import unittest
from unittest.mock import patch

import vmic_status as monitor


class Client:
    base = 0x10000000
    shared = 0x20000000

    def __init__(self):
        self.firmware = 960
        self.bad_code = False
        self.bad_mapping = False
        self.change = False
        self.reads = []
        self.slot_reads = 0

    def version(self):
        return dict(firmware_raw=self.firmware, platform=4)

    def processes(self):
        return [dict(name='orbis_audiod.elf', pid=39)]

    def info(self, pid):
        return dict(path='/system/sys/orbis_audiod.elf', title_id='NPXS20973')

    def maps(self, pid):
        return [dict(name='executable', start=self.base, end=self.base + 0x400000, prot=5),
                dict(name='wrong' if self.bad_mapping else '/vmicDdShmAin',
                     start=self.shared, end=self.shared + 0xc000, prot=3)]

    def read(self, pid, address, length):
        self.reads.append((address, length))
        if address == self.base + 0x28840:
            return (b'X' if self.bad_code else b'C') * 32
        if address == self.base + 0x2c0410:
            return struct.pack('<Q', self.shared)
        if address == self.base + 0x2c0418:
            return b'\x01'
        slot, offset = divmod(address - self.shared, monitor.STRIDE)
        if 0 <= slot < 4 and offset == 0 and length == 8:
            self.slot_reads += 1
            active = int(slot == 0)
            owner = 123 if active else 0
            if self.change and self.slot_reads > 4 and active:
                owner += 1
            return struct.pack('<II', active, owner)
        if 0 <= slot < 4 and offset == 0x2018 and length == 12:
            return struct.pack('<III', 1, 16000, 128)
        if 0 <= slot < 4 and offset == 0x2030 and length == 4:
            return struct.pack('<I', 0x40100 + slot)
        raise AssertionError('Unexpected read, possibly PCM')


class SnapshotTests(unittest.TestCase):
    def setUp(self):
        self.hashes = patch.dict(monitor.CODE_HASHES,
                                {0x28840: hashlib.sha256(b'C' * 32).hexdigest()},
                                clear=True)
        self.hashes.start()
        self.addCleanup(self.hashes.stop)
        self.client = Client()

    def test_metadata_only(self):
        result = monitor.snapshot(self.client)
        self.assertFalse(result['pcm_read'])
        self.assertEqual(result['slots'][0]['owner_pid'], 123)
        self.assertEqual(self.client.slot_reads, 8)

    def test_firmware_fails_before_reads(self):
        self.client.firmware = 1100
        with self.assertRaisesRegex(ValueError, '9.60'):
            monitor.snapshot(self.client)
        self.assertEqual(self.client.reads, [])

    def test_mismatched_code_blocks_slots(self):
        self.client.bad_code = True
        with self.assertRaisesRegex(ValueError, 'code mismatch'):
            monitor.snapshot(self.client)
        self.assertEqual(self.client.slot_reads, 0)

    def test_pointer_must_belong_to_named_mapping(self):
        self.client.bad_mapping = True
        with self.assertRaisesRegex(ValueError, 'named shared mapping'):
            monitor.snapshot(self.client)
        self.assertEqual(self.client.slot_reads, 0)

    def test_changing_slots_rejected(self):
        self.client.change = True
        with self.assertRaisesRegex(ValueError, 'changed during observation'):
            monitor.snapshot(self.client)


if __name__ == '__main__':
    unittest.main()
