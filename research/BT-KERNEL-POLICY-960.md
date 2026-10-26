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

## Historical 0.16 relaunch

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

## Extended predicate and dispatch checks

The host checker now supports three independently fingerprinted slices:

```
python3 research/check_bt_policy_host.py PRIVATE_RX_CAPTURE
python3 research/check_bt_policy_host.py --secondary PRIVATE_RX_CAPTURE
python3 research/check_bt_policy_host.py --dispatch PRIVATE_RX_CAPTURE
```

The primary predicate passes 60 cases; the secondary predicate passes 78.
Both synthetic candidates mask address comparisons to 48 bits and preserve
scratch registers. Cases include nonzero upper address metadata, an unrelated
address, wrong device class, source-only devices, mixed HID/audio and stock
vendor/product admissions. The second check begins at 0x59f96e after the timer
update and stops at local acceptance/rejection stubs; it does not exercise the
initial connection-state guard, capacity check or subsequent profile setup.

Twelve further cases execute the original dispatcher at 0x5963d0, replacing
transport destinations with local result stubs. A synthetic remote-address bit
skips its timer helper, and teardown is not entered. For services 0x38 it selects
service 8 first, then service 0x10 when 8 is active or excluded. The dispatcher
also routes service 4 and HID 0x40. No dispatch occurred for isolated bits 1 or 2
in this path; this does not establish their names or prove absence of another
microphone path. No codec negotiation or audio transport runs in this test.

The special record flag is not an acceptable bypass: 0x5b2e44..0x5b2eb8 scans
other connections and can call disconnect on another matching record. The
candidate instead enters the normal capacity-counted path. Simply setting the
existing-state bit would also skip bookkeeping and is not an implementation.

## Live installation remains unresolved

Update 2026-10-25: a one-shot kernel-entry probe and an eight-CPU counting
rendezvous have now succeeded in the running BlueHEN GUI, using a temporary
kernel-owned executable allocation. See
[kernel installation prerequisites](KERNEL-INSTALL-PREREQUISITES-960.md) for
physical evidence and the remaining interrupted-instruction / trampoline-lifetime
requirements. The paragraph below records the earlier state before those probes.

The pinned debugger's [kernel handler](https://github.com/Pharaoh2k/ps4debug-NG/blob/f941e2d31f83d5e1a3b64690a46787703498171a/debugger/source/kern.c)
provides base/read/write operations, not a synchronized code-patch transaction.
Its [kernel read/write implementation](https://github.com/Pharaoh2k/ps4debug-NG/blob/f941e2d31f83d5e1a3b64690a46787703498171a/kdebugger/source/hooks.c)
must not be mistaken for multiprocessor quiescence. The sibling Receiver has a
syscall-11 callback wrapper, but its availability and safe code-patch machinery
have not been validated in this BlueHEN process. No such callback was invoked
in this investigation.

Before the first live patch: resolve and validate a 9.60 kernel execution and
CPU-synchronization path, allocate kernel-owned executable trampoline storage,
verify both original sites inside the synchronized section, and journal the
installed bytes/ownership before permitting a selected connection. Removal
must exclude active callbacks before freeing storage. Never place a persistent
kernel jump into the GUI's user mapping or equate host predicate success with
safe installation. The current GUI, installed package and kernel are unchanged.

After admission: verify AVDTP negotiation and bounded speaker output, then
locate/implement HFP service negotiation and SCO/eSCO transport for microphone,
and finally test AudioIn/party routing. These remain functional requirements,
not completed features. The HP206 cache observed here advertises only the
0x38 service mask; Android HFP support does not prove PS4 HFP discovery.
