"""One-shot 9.60 Vmic lifecycle experiment; transport supplied by caller.

Caller must verify firmware, isolated process identity, exact function code,
and a live non-busy RPC session. No routing, PCM or radio operations occur.
An RPC exception leaves an unknown outcome; never retry automatically.
"""


def run_once(pid, create, destroy, observe, record):
    def owned(slots):
        return [s for s in slots if s['active'] and s['owner_pid'] == pid]

    before = observe()
    if owned(before) or not any(not s['active'] for s in before):
        raise ValueError('Existing owned microphone or no free slot')
    record('before', before)
    record('create_submitting', {'args': [0, 128, 16000, 1]})
    result = create(0, 128, 16000, 1) & 0xffffffff
    record('create_returned', result)
    after_create = observe()
    record('after_create', after_create)
    mine = owned(after_create)
    if len(mine) > 1:
        raise ValueError('Unexpected multiple owned slots; no speculative cleanup')
    if not mine:
        if result < 0x80000000:
            raise ValueError('Successful return without observable owned slot')
        return {'created': False, 'create_result': result, 'owned_slots_remaining': 0}
    slot = mine[0]
    handle = slot['handle']
    if handle & 0xffff00ff != 0x40000 + slot['slot']:
        raise ValueError('Unexpected owned handle layout')
    # Create can hide its handle with a later Mbus error. Recover only the
    # exact handle observed in a newly owned slot, never guess one.
    format_matches = (slot['frames'], slot['sample_rate'], slot['channels']) == (128, 16000, 1)
    latest = owned(observe())
    if len(latest) != 1 or latest[0]['handle'] != handle:
        raise ValueError('Ownership changed before cleanup')
    record('destroy_submitting', {'handle': handle, 'owner_pid': pid})
    destroyed = destroy(handle) & 0xffffffff
    record('destroy_returned', destroyed)
    after_destroy = observe()
    record('after_destroy', after_destroy)
    remaining = len(owned(after_destroy))
    return {'created': True, 'create_result': result, 'observed_handle': handle,
            'returned_handle_matches': result == handle, 'format_matches': format_matches,
            'destroy_result': destroyed, 'owned_slots_remaining': remaining,
            'lifecycle_pass': result == handle and format_matches and destroyed == 0 and remaining == 0}
