# Bluetooth admission policy on the user's PS4 9.60

## Evidence and capture

The earlier claim that kernel reads were unavailable was incomplete. Doom3's
existing `worker_scope.py` already uses ps4debug-NG commands BDCC0001 (base) and
BDCC0002 (read). The pinned NG1.3.2 source commit is
`f941e2d31f83d5e1a3b64690a46787703498171a`:
[kern.c](https://github.com/Pharaoh2k/ps4debug-NG/blob/f941e2d31f83d5e1a3b64690a46787703498171a/debugger/source/kern.c),
[hooks.c](https://github.com/Pharaoh2k/ps4debug-NG/blob/f941e2d31f83d5e1a3b64690a46787703498171a/kdebugger/source/hooks.c).
The read handler uses memcpy without validating mappings, so reads were confined
to verified ELF segments and narrowly derived driver fields. No kernel write or
new debugger load was performed.

A fresh base query and 4096-byte read yielded an ELF64/x86-64 header. Its single
RX PT_LOAD spans 13574192 bytes (0xcf2030). The segment was copied in 65536-byte
chunks, with the first page identical before and after. This is a running,
possibly already patched kernel image, not an unmodified distribution image.
Private image SHA256:
`14ef08101c3716cc774ee2098a01b7278213d92768c7172e99b6309eebe31596`.
Private journals: kernel-header-read.json, kernel-text-read.json,
kernel-960-text.bin, capture_kernel_text.py. Addresses are session-specific;
all offsets below are relative to that validated kernel base.

A partial Git checkout under review/ps4debug-ng-source was abandoned when the
large download stalled. It is NOT a checkout of the pinned commit. Source review
used the exact commit's raw files. No proprietary kernel/module bytes enter Git.

## Error-generating branches

The RX image contains two immediate instances of 0x802f3403:

| Function | Immediate offset | Relevant admission checks |
| --- | --- | --- |
| 0x59f940 | 0x59f9da | ID source 2 and selected vendor/product pairs |
| 0x5b2b50 | 0x5b2f28 | Existing connection bit, special record flag, selected vendor/product pairs, or services exactly 0x40 |

In the second function, the gate at 0x5b2dad checks connection+0x19 bit 1, then
record+0x26 bit 4, then record ID source/vendor/product. The fallback at 0x5b2ef4
requires record services+0x20 to equal 0x40. Rejection enters 0x5b2f24; the accepted
branch enters 0x5b2efa and retains a capacity check. Function 0x5962b0 turns the
negative result into status 0xff and emits event 5 through 0x593b80, matching our
observed event type, status and value.

The data indicates a software admission policy. It does NOT identify the live
instruction pointer at failure, prove that this is the only obstacle, or establish
that allowing an audio endpoint yields usable streaming. Both error sites must
be accounted for before any live change.

## Read-only target observation

Code-derived global 0x2606098 lies inside the ELF writable segment and contains
the driver context pointer. The record pool is context+0x18da8, 32 records with
stride 0x408. Only 0x30-byte headers were read; no names or key material was read
or logged. Target matching used EutherBlue's recorded address in memory, without
logging it. Code fingerprints and context-pointer stability were checked.

The HP206 matched record slot 8:

| Field | Observed value |
| --- | --- |
| Class | 0x240404 |
| Services | 0x38: A2DP sink and AVRCP controller/target |
| Internal flags | 0x882000 |
| Vendor/product/source | 0/0/0 |
| Registered flag candidate | clear |

These are cached discovery/service data, explaining why public registered-info
queries can return no matching record while the kernel knows the device's
services. The internal layout and flag interpretation are reverse-engineered.
The 7 connection slots (context+0x42a8, stride 0x1588) were inspected separately:
only slot 0, referring to controller record 5, remained active. Other slots were
empty. Private evidence: bt-cache-headers.json, bt-connection-slots-read.json.

## Offline executable prototype

`check_bt_policy_host.py PRIVATE_RX_CAPTURE` fingerprints the relevant two pages
before compiling/running check_bt_policy_host.c in an isolated Linux/x86-64
child. It executes the actual gate instructions with synthetic connection state,
local exit stubs, and observed HP206 service/flag values. No console connection
or firmware input writes occur. Native execution replaced a stalled Unicorn
download; no emulator result is claimed.

The prototype preserves the stock HID equality check. Its additional branch
admits only a synthetic selected address, audio/video class, A2DP sink bit,
and no HID device bit. All other cases retain stock behavior. Forty combinations
passed: stock/prototype, selected/other address, HP206, HID, mixed HID/audio,
Sony controller, generic sink, special/existing-state branches, wrong class,
source-only and no services. This validates the local admission predicate only.

The prototype is NOT a console installer. A live implementation still needs:

- Confirm the relevant runtime path and state for the selected headset.
- Account for the other 0x802f3403 site and downstream profile setup.
- A target-scoped trampoline with verified ownership, safe multiprocessor
  installation/removal, lifetime handling and restoration. A six-byte branch
  replacement is not an atomic write; a raw debugger write is insufficient.
- Preserve the existing controller path and verify cleanup before any audio test.

There are service 4/8 branches downstream at 0x5963d0..0x59659f leading to 0x5aa300.
They are code evidence of profile handling, not successful A2DP transport.
Microphone transport remains a separate unresolved requirement.

## Latest console state

The exact 0.16 package was relaunched without reinstalling it after verifying
no own process/BGFT marker. Previous logs were archived and old PID 120 gate
files removed. New PID 124 passed auth/export gates, controller readiness and
first presentation. Ticket 9 consumed once (0x60001e0d). No reboot occurred.

A bounded 110-second read-only monitor of the target's connection state completed
without seeing a target attempt. The user has been asked to run Triangle,
select UGreen with X, Square, then X on confirmation. The GUI may idle-exit after
five minutes. Do not relaunch until process/slot/BGFT cleanup is verified.
Private journals: service-ui-v16-policy-run.json, service-ui-v16-policy-console.log,
launch-BLHN00007-device-ui-v16-policy.json, bt-target-policy-observation.json.

## Follow-up physical observation

The policy5 run captured the selected HP206 in connection slot 1, cache slot 8.
Connection flags progressed 0x1 -> 0x11 -> 0x13; record flags changed from
0x880000 to 0x882000. Services remained 0x38 and vendor/product/source were zero.
Neither the existing-state bypass nor the special-record bit was set. The GUI
accepted confirmation (reply 0), then received event 5/status 255/value
0x802f3403. Disconnect and queue/descriptor cleanup succeeded.

This confirms that the live target state fails the examined admission predicate;
it is not an instruction-pointer trace and does not distinguish the two error
sites. The preceding policy3 attempt instead returned status 4/value 0 without
confirmation, so application result 64 alone must not identify the cause.
Private evidence: bt-target-policy5-observation.json and
service-ui-v16-policy5-result.log. No kernel change was made.
