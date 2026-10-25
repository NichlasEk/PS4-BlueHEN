import struct
import unittest
from auth_profile import validate_profile, verify_signature


class Profiles(unittest.TestCase):
    def setUp(self):
        self.base = struct.pack('<Q', 0x3800000000000011) + bytes(128)
        self.profile = bytearray(self.base)
        self.profile[15] |= 0x40

    def test_only_target_bit(self):
        self.assertEqual(validate_profile(self.base, self.profile), 0x3800000000000011)

    def test_reject_other_changes(self):
        for offset in [0, 8, 16, 40, 135]:
            changed = self.profile.copy()
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                validate_profile(self.base, changed)

    def test_reject_wrong_size_and_already_system(self):
        for base, profile in [(self.base[:-1], self.profile),
                              (self.base, self.profile[:-1]),
                              (self.profile, self.profile),
                              (self.base, self.base)]:
            with self.assertRaises(ValueError):
                validate_profile(base, profile)

    def test_signature_missing_duplicate_or_corrupted(self):
        sig = struct.pack('<Q', 136) + self.profile
        verify_signature(b'prefix' + sig + b'suffix', self.profile)
        for data in [b'', sig + sig, sig[:-1]]:
            with self.assertRaises(ValueError):
                verify_signature(data, self.profile)


if __name__ == '__main__':
    unittest.main()
