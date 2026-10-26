# Established audio links without streaming, firmware 9.60

The October 26 enabled test passed primary admission, returned connect event
status 0, created a registered headset record, and the user heard the headset
announce a connection. BlueHEN 0.18 nevertheless timed out because its UI only
accepted states 3 and 5. That was an incorrect completion condition for audio.

The platform enumeration names value 6 STOPPING, but the native query does not
require an active stop operation to return it. The ioctl dispatch table entry
at 0xaca61c maps 0x80086203 to 0x5989b1, which calls 0x594bf0. After finding the
selected connection, the query evaluates:

- Missing connection or inactive driver: 1.
- Connection flags at +0x18 include 0x40: 4 (disconnecting).
- Connection flags lack 0x04000000: 2 (connecting).
- Established ordinary device without service bits 4/8: 3.
- Established generic audio device: 5 if connection+0x12dc has bits 4/8;
  otherwise 6. The latter is an established link without active streaming.
- Sony source=2/vendor=0x054c/product=0x05c4 uses separate +0x642/+0x643 bytes.

This is the native query's classification, not proof that a codec is negotiated
or that PCM can be delivered. Returning 6 after successful connection must not
be treated as a failed pairing, nor advertised as functioning audio.

`check_bt_state_host.py PRIVATE_CAPTURE` verifies the capture's complete hash
before executing the original function with synthetic context, connection and
record data. Only locking and connection lookup use host stubs. All 107 cases
passed, including generic source/sink/controller service sets, disconnecting,
connecting, active/idle streams, and the Sony special case. The live console's
entire query, ioctl call site and table entry matched the private capture.
Firmware bytes and device identifiers remain outside Git.

Version 0.19 enters the held-link loop at state 6 only after the selected
attempt's status-zero connect event. A state-6 response without that event
still times out. The loop accepts 3/5/6 and retains the existing user/error
cleanup. It does not initiate audio, reconnect automatically, or patch the
kernel itself. Seven app tests pass with 28 connection scenarios, including
idle-audio success and idle-audio without completion. Target build, profile,
signature and package validation passed; hardware acceptance is pending.

Exact candidate: build/service-ui-v19/IV0000-BLHN00007_00-BLUEHENSERVICE01.pkg,
6619136 bytes, SHA256
3dbe60ced83775520131002ee63651eb550da149db643e7453430c3d37ac27c5.

Next transport boundary: trace the stream enable/configuration operation before
attempting audio. The native audio-daemon requests 0x80106212, 0x80206216 and
0x80206217 route to kernel handlers at 0x598d45, 0x59850c and 0x59873f. The last
two call 0x5966b0 and 0x5967c0; their envelope alone does not establish PCM format
or initialization requirements. No such request has been sent by this change.

Further static trace: 0x80206216 passes address, buffer, count, and a fourth
32-bit selector (0..4) to 0x5966b0, then 0x5a7b60. For a generic A2DP sink,
selector zero takes the stop/clear path; a nonzero selector with neither active
nor pending stream bit 8 calls 0x5a8a80 to start that service before the later
buffer path. Thus sending silence with selector zero would not start streaming.
The selector's exact PCM-format meaning and buffer/count units remain to be
verified against callers. Request 0x80106212 reaches 0x5b7fa0, masks its byte
argument to seven bits and emits an AVRCP operation; it is not the stream-start
call. These are static findings only, not executed audio operations.
