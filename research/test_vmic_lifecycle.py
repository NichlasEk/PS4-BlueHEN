import unittest
from vmic_lifecycle import run_once


class LifecycleTests(unittest.TestCase):
    def scenario(self, result=0x40100, timeout=False, foreign=False):
        slots = [dict(slot=0, active=False, owner_pid=0, handle=0,
                      frames=128, sample_rate=16000, channels=1)]
        calls = []
        def create(*args):
            calls.append(('create', args))
            slots[0].update(active=True, owner_pid=999 if foreign else 123, handle=0x40100)
            if timeout:
                raise TimeoutError('Unknown remote outcome')
            return result
        def destroy(handle):
            self.assertEqual(slots[0]['owner_pid'], 123)
            calls.append(('destroy', handle))
            slots[0].update(active=False, owner_pid=0)
            return 0
        return slots, calls, create, destroy

    def run_case(self, **kwargs):
        slots, calls, create, destroy = self.scenario(**kwargs)
        result = run_once(123, create, destroy, lambda: [dict(s) for s in slots], lambda *a: None)
        return result, calls

    def test_success_and_cleanup(self):
        result, calls = self.run_case()
        self.assertTrue(result['lifecycle_pass'])
        self.assertEqual([c[0] for c in calls], ['create', 'destroy'])

    def test_hidden_handle_after_error_is_cleaned(self):
        result, calls = self.run_case(result=0x809b0001)
        self.assertFalse(result['lifecycle_pass'])
        self.assertFalse(result['returned_handle_matches'])
        self.assertEqual(calls[-1], ('destroy', 0x40100))

    def test_foreign_slot_is_never_destroyed(self):
        with self.assertRaisesRegex(ValueError, 'without observable'):
            self.run_case(foreign=True)

    def test_timeout_is_not_retried_or_blindly_cleaned(self):
        slots, calls, create, destroy = self.scenario(timeout=True)
        with self.assertRaises(TimeoutError):
            run_once(123, create, destroy, lambda: [dict(s) for s in slots], lambda *a: None)
        self.assertEqual([c[0] for c in calls], ['create'])

    def test_existing_owned_slot_blocks_create(self):
        slots, calls, create, destroy = self.scenario()
        slots[0].update(active=True, owner_pid=123)
        with self.assertRaises(ValueError):
            run_once(123, create, destroy, lambda: slots, lambda *a: None)
        self.assertEqual(calls, [])
