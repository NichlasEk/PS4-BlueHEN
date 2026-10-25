"""Validate a private, measured authorization profile for the owned test app."""
import struct

SIZE = 136


def validate_profile(baseline, profile):
    if len(baseline) != SIZE or len(profile) != SIZE:
        raise ValueError('Expected exactly 136 authorization bytes')
    if baseline[15] & 0x40:
        raise ValueError('Baseline already has the system bit')
    expected = bytearray(baseline)
    expected[15] |= 0x40
    if profile != expected:
        raise ValueError('Profile must change only caps[0] bit62')
    paid, = struct.unpack_from('<Q', profile)
    if not 0 < paid < (1 << 63):
        raise ValueError('paid must fit the builder signed argument')
    return paid


def verify_signature(eboot, profile):
    signature = struct.pack('<Q', SIZE) + profile
    if eboot.count(signature) != 1:
        raise ValueError('Expected one exact authorization signature')
