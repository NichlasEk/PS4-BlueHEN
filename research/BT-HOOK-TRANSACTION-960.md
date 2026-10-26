# Single-instruction Bluetooth hook transaction, firmware 9.60

Status: **disabled installation and complete restoration physically verified**
on 2026-10-25 in the freshly opened GUI. Both SMP masks were 0xff for installation
and restoration. Independent readback matched all three original 4 KiB pages.
The initial disabled cycle kept both hook counters at zero. A later enabled
connection attempt is recorded below. This proves the transaction, not headset
connection or physical trampoline execution.
The accepted GUI package remained unchanged and alive afterward.

The first direct-jump preparation returned ERANGE (34): the allocator placed
the page beyond rel32 reach. Its release returned and the user test page was
unmapped. No instruction was patched by that attempt. The accepted second
variant uses two verified nearby pointer slots and full-width indirect jumps.

## Different patch windows

The initial host prototype replaced a compare and its conditional branch
together. That leaves a possible saved instruction pointer inside the replaced
window. This version instead replaces exactly one original instruction:

| Site | Original operation | Length | Stock continuation |
| --- | --- | --- | --- |
| 0x5b2dad | Test connection byte +0x19, bit 0 | 6 | 0x5b2db3 |
| 0x59f966 | Store timer return EAX to connection +0x1128 | 8 | 0x59f96e |

Each replacement is a six-byte RIP-relative indirect jump, with two unused NOP
bytes at the secondary site. The trampoline reproduces the overwritten instruction before its extra
predicate. Stock paths retain register/flag behavior, existing/special paths and
capacity checks. The new admission remains restricted to one 48-bit target,
audio/video class, A2DP sink set and HID clear. The secondary timer store executes
even when the extra predicate accepts the target.

Precise restart is defined at instruction boundaries in the
[AMD64 system programming manual, section 8.1](https://docs.amd.com/api/khub/documents/sD1_QL~h4Afq2_tvzxqqSQ/content).
The design inference is that, with these complete non-REP instructions, a normal
interrupted execution is either at the original start or beyond the window.
Both remain valid after installation. When removing the jump, its padding is
unreachable by fallthrough; a saved pointer is at the jump or in resident code.
This depends on ordinary compiled control flow with no interior entry. A linear
disassembly scan of the private RX image found no direct branches into either
window's interior; it found the two known jumps to 0x5b2dad. This scan does not
prove all indirect targets. Host tests additionally execute both stock
continuations as if a CPU had already completed the old instruction.

## Synchronization and lifetime

The controller prepares a kernel-owned 16 KiB allocation before publication and
returns its identity for a private journal. The two jumps reference aligned
eight-byte pointer slots at 0x59f658 and 0x59f938. These are original NOP padding
after terminal control flow (return/panic-UD2, and unconditional jump), before
the next function. A full RX linear scan found no direct branch or RIP-relative
reference into these padding regions. This is static layout evidence, not a
proof against arbitrary computed jumps. Whole-page fingerprints are checked
before use. The pointers contain full 64-bit addresses, so the allocated
trampolines need not be within 2 GiB of the firmware. All four windows are
compared, journaled and restored together; pointer slots are populated before
entry jumps and restored after original instructions.

Inside a normal SMP rendezvous, an atomic election chooses one writer. That
writer compares all four complete expected windows before changing any, preserves
CR0, clears only WP during stores, reads back both sites, and restores CR0.
Failed verification attempts to restore all prior windows; failed restoration
is a separate error, never reported as success. The normal action barrier then
precedes CPUID on every participating CPU. This follows the publication/consumer
serialization ordering described in
[Intel's cross-modifying-code algorithm](https://cdrdv2-public.intel.com/774475/252046-sdm-change-document.pdf).
The physical acceptance check must require both visit and serialization masks
to match all eight CPUs, followed by fresh independent code readback.

Management commands are serialized by an owner-checked atomic lock. Installation
defaults to the original admission behavior (`enabled=0`); activation is a
separate command. The initial physical runner permits only prepare, disabled
installation and restoration. It does not enable admission or initiate pairing.

Restoration deliberately does **not free published resident storage**. A paused
trampoline can therefore finish even if it was interrupted before incrementing
any counter. This costs one 16 KiB allocation until reboot. The private owner
journal must be recovered and reused; do not create repeated allocations per
boot. The retained object is independent of GUI lifetime. Immediate revocation
of an already-decided admission is not claimed. No reboot is performed by any
of these tools.

## Validation and artifacts

```
python3 research/build_bt_hook.py --out build/bt-hook-960-indirect-v2 \
    --image /absolute/path/to/private/kernel-960-text.bin
```

The destination must be new. The capture's full SHA256 is checked before any of
its instructions execute in a Linux child process. No firmware is bundled.

* 1920 predicate/continuation checks: 16 cases, three address variants, two sites
  and two entry modes, across stock / installed-disabled / enabled / disabled /
  restored phases. Includes upper address metadata, controller paths, the
  original timer store, special/existing branches and capacity-path destinations.
  The 960-case suite runs both with normally linked trampolines and with the exact
  generated resident blob mapped at 1 TiB, well beyond direct-jump range.
* 19 transaction scenarios run the actual transaction function with privileged
  CPU/CR0 access substituted. Eight host threads exercise writer election and
  the serialization stage. Tests cover all four window conflicts, duplicate commands,
  activation/deactivation, restoration, successful rollback and failed rollback.
  Full-buffer comparisons detect writes outside the expected windows; CR0 must
  return to its initial value on each worker. These are host tests, not real
  instruction-cache or kernel-exception tests.
* No unresolved symbols or runtime relocations; the original windows each
  disassemble as one complete instruction. Disassembly still requires review
  for absolute references before a live run.

Physically tested build `build/bt-hook-960-indirect-v2/hook.bin`: 8416 bytes, SHA256
`10815767bd41a6bdbd7e44e5c3d6d9a4efee9c9dcb594912a2619b297283d772`.
This is an internal research blob, **not a PS4 PKG or a standalone BinLoader
payload**. Do not send it to port 9090. Existing installed 0.17 is unchanged.

Private successful runner and journals: `review/hook-indirect-20261025/`:
prepare.json, install-disabled.json, restore.json,
independent-restoration-check.json and post-test/. Historical direct-jump
attempts remain in `review/hook-transaction-20261025/`. Before any
further operation, rediscover the GUI PID, verify its accepted package and
fresh startup log, validate RPC state and all function fingerprints, match the
selected device's cache header without recording its address, and confirm that
it has no active connection. The code and original/expected patch windows must
be revalidated for each operation. A submitted command is never blindly retried
after a timeout. A failed preflight with zero invocations is not a failed kernel
transaction.

The first transaction cycle is complete; the object is restored and disabled,
with generation 2 and one 16 KiB allocation intentionally retained. Reuse its
journaled identity instead of preparing another object. Next is a supervised
target-enabled connection test with planned cleanup. The current payload has no
autonomous expiry: a host timeout alone is not a kernel-enforced lease. Do not
claim it disables itself if the host or transport disappears. Actual audio and
microphone transport remain separate work.


## First enabled physical connection attempt (2026-10-25)

The existing v2 resident was reused for a supervised install/enable/disable/
restore cycle. All four operations returned zero, with CPU masks ff/ff and
successful temporary user-page cleanup. The final state is generation 6,
installed=0 and enabled=0; the single resident allocation remains retained.
Private evidence is in `review/hook-live-attempt1/`.

The user attempted the connection to the correct scoped headset. Discovery and
StartConnect succeeded, but event type 5/status 4 ended the attempt before any
confirmation event. Captured platform metadata field 2300 identifies status 4
as `SCE_BT_STATUS_PAGE_TIMEOUT`. The selected live UI record matched the scoped
target without recording its address. Both admission hit counters stayed zero,
so this attempt did not exercise either patched policy decision. It does not
establish bypass success or rejection. The reason for the page timeout remains
unresolved; fresh pairing mode and competing connections should be checked
before another supervised attempt. Audio and microphone transport are untested.

## Latest restored state and reproducibility (2026-10-26)

A second supervised 180-second window on October 25 produced no new connection
events. Disable and restore completed, leaving installed=0/enabled=0 at
generation 10. This supersedes the intermediate generations above. It is a
historical observation; a new session requires fresh identity and byte checks.

On October 26, a separate verification build reproduced the exact accepted
8416-byte blob and SHA256 above. All 1920 predicate checks, 19 transaction
scenarios, and seven service-probe tests passed. Entry/SMP probes also reproduced
their previously tested hashes. Original hardware-tested artifacts were preserved.
Successful pairing, headset audio and microphone transport remain unverified.

## Enabled hardware result, 2026-10-26: admission passes; audio state unresolved

The same-boot resident was independently matched by owner, complete immutable
code, and all original patch windows before reuse with GUI 0.18. One supervised
install/enable cycle reached generation 12. During the user's connection attempt
the primary counter incremented once and primary_allowed became 1. Secondary
hits/allowed remained zero. Confirmation returned 0, then event 5 reported
status 0/value 0x38, replacing the preceding unpatched 255/0x802f3403 result.

The user reported that the headphones announced they were connected. The public
registered-info query subsequently contained the selected headset (previously
absent), class 0x240404, services 0x200038, flags 0x300. These are pairing/link
progress and a newly observed registration, not proof of usable PS4 audio.

The PS4 connecting-info query stayed at 6 throughout the post-confirmation wait.
Local platform metadata maps field 2288/value 6 to STOPPING; connected and
streaming remain 3 and 5. The exact internal cause and transport state are not
yet established. GUI 0.18 timed out with result 60 and explicitly disconnected
the selected headset; cleanup verified state 1 and retained the registration.
Thus the app did not retain this successful-event link as an audio-ready session.
Do not reinterpret state 6 as streaming merely to show success in the UI.

Disable/restore completed at generation 14, installed=0/enabled=0. Independent
readback matched all three full original kernel pages; the GUI remained alive.
The one 16 KiB resident remains retained until reboot. Private evidence:
`review/hook-live-20261026/` including supervision, operation journals, final log
and independent-restoration-check; prior unpatched attempts and exact selected
target verification are under `review/status255-20261026/`.

Next: determine why the post-admission profile/state path returns STOPPING and
whether AVDTP setup requires a separate audio consumer. Trace the native state
query and downstream profile dispatch before adding calls or changing cleanup.
Headset output and microphone/party routing remain unverified.
