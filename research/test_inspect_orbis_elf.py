"""Synthetic ELF fixtures only; no firmware files needed or redistributed."""
import struct
import unittest
from inspect_orbis_elf import inspect, nid


def fixture():
    data = bytearray(1024)
    data[:6] = b'\x7fELF\x02\x01'
    struct.pack_into('<H', data, 18, 62)
    struct.pack_into('<Q', data, 32, 64)
    struct.pack_into('<HH', data, 54, 56, 3)
    struct.pack_into('<IIQQQQQQ', data, 64, 1, 5, 256, 4096, 0, 64, 64, 16)
    struct.pack_into('<IIQQQQQQ', data, 120, 0x61000000, 0, 512, 0, 0, 256, 256, 8)
    struct.pack_into('<IIQQQQQQ', data, 176, 2, 0, 768, 0, 0, 80, 80, 8)
    strings = b'\0DVTn+iMSpBM#A#A\0'
    data[512:512 + len(strings)] = strings
    struct.pack_into('<IBBHQQ', data, 576, 1, 0x12, 0, 1, 4096, 64)
    for i, pair in enumerate([(0x61000035, 0), (0x61000037, len(strings)),
                               (0x61000039, 64), (0x6100003f, 24), (0, 0)]):
        struct.pack_into('<QQ', data, 768 + 16 * i, *pair)
    return data


class ElfMetadataTests(unittest.TestCase):
    def test_known_nids(self):
        self.assertEqual(nid('sceAudioInVmicCreate'), 'DVTn+iMSpBM')
        self.assertEqual(nid('sceAudioInVmicWrite'), '4kHw99LUG3A')
        self.assertEqual(nid('sceAudioInVmicDestroy'), '3ULZGIl+Acc')

    def test_named_export_and_missing(self):
        r = inspect(fixture(), ['sceAudioInVmicCreate', 'missing'])
        self.assertEqual(r['unmatched'], ['missing'])
        self.assertEqual(r['exports'][0]['file_offset'], 256)
        self.assertEqual(r['exports'][0]['virtual_offset'], 4096)
        self.assertEqual(r['exports'][0]['prefix_bytes'], 32)

    def test_import_is_not_export(self):
        b = fixture()
        struct.pack_into('<H', b, 582, 0)
        self.assertEqual(inspect(b, ['sceAudioInVmicCreate'])['exports'], [])

    def test_truncated_and_wrong_format(self):
        for b in [b'', b'\x4f\x15\x3d\x1d', fixture()[:200], fixture()[:800]]:
            with self.subTest(length=len(b)), self.assertRaises(ValueError):
                inspect(b, [])

    def test_tables_cannot_escape_dynlib_segment(self):
        b = fixture()
        struct.pack_into('<Q', b, 808, 250)
        with self.assertRaisesRegex(ValueError, 'outside dynlib'):
            inspect(b, [])

    def test_unterminated_symbol(self):
        b = fixture()
        b[512:544] = b'A' * 32
        with self.assertRaisesRegex(ValueError, 'symbol string'):
            inspect(b, [])

    def test_short_executable_segment_has_no_prefix_hash(self):
        b = fixture()
        struct.pack_into('<Q', b, 96, 8)
        r = inspect(b, ['sceAudioInVmicCreate'])
        self.assertNotIn('prefix_sha256', r['exports'][0])


if __name__ == '__main__':
    unittest.main()
