# Native automatic admission, BlueHEN 0.20 / firmware 9.60

0.20 moves the tested target-scoped Bluetooth hook lifecycle into the GUI.
The user selects a headset and presses Square. The app verifies the selected
idle audio sink, prepares or recovers its own kernel resident, installs the
hook disabled, and enables it immediately before StartConnect. On connection
failure, cancellation, disconnect or normal exit it observes current state,
disables admission, restores all four windows under the existing SMP protocol,
and verifies the restored windows. No desktop supervisor, debugger RPC, manual
timing, external activation file or private headset address embedded in code is
required. The existing firmware/auth-profile checks remain mandatory.

This adds admission management, not audio transport. Version 0.19's established
idle-audio state handling remains: state 6 is retained only after a successful
connect event and is not advertised as active sound.

## Native validation and recovery

`bt-auto-control.c` wraps the existing transaction with native SHA-256 checks
of the syscall trampoline, SMP routines, allocator routines and all three
Bluetooth code pages. Only the four individually compared patch windows are
normalized during page hashing. Foreign changes fail before writes. A retained
resident's complete immutable code, owner, base, version, target and clock
frequency are checked before use. Preparing/enabling requires exactly one
matching cached audio sink and no active target connection. Controller paths
and original capacity behavior remain unchanged.

The GUI uses the firmware's sysctl KERN_BOOTTIME and sceKernelGetTscFrequency.
Its private 0600 checksummed journal is `/data/bluehen/automatic-v1.state`.
It records an allocation intent with fflush/fsync/rename before preparation,
then persists the returned resident before any code publication. If the app
dies between allocation and durable identity, subsequent same-boot attempts
refuse to allocate again; reboot clears that incomplete-session condition.
A changed boot time discards saved pointers before attempting to access them.
Corrupt journals and incompatible same-boot blob versions fail closed.

The same resident is reused across connections, selected targets and ordinary
app restarts. An existing same-boot resident is inspected and restored during
app initialization. The earlier manual v2 resident is not overwritten or freed;
this new format owns its own single allocation. Both allocations, if present
in an upgrade session, remain until console reboot. Published code is never
freed while a trampoline could still be returning through it.

## Crash/suspension boundary

The automatic trampoline additionally checks a kernel-resident TSC deadline
before granting extra admission. The app requires invariant-TSC support and a
sane native frequency, renews the two-second deadline approximately every
quarter second through its UI pump, and stops renewing during cleanup.
A stopped, suspended or crashed GUI therefore stops granting additional
admission after expiry. Expiry does **not** remove the jump instructions or
rewind an already admitted connection; those instructions continue executing
the original policy through the retained trampoline. Physical code restoration
requires normal cleanup, recovery by the next compatible app launch, or reboot.
Do not claim that a process crash triggers an autonomous SMP restore.

## Build and verification

From `BlueHEN/`, with the private capture and existing auth profile inputs:

```
python3 -B research/build_bt_auto.py --image /private/kernel-960-text.bin --out build/bt-auto-new
BLUEHEN_AUTO_BUILD=build/bt-auto-new python3 -B research/service-probe/build_device_ui_pkg.py --out build/service-ui-v20-new
```

Keep BLUEHEN_BASELINE_AUTH and BLUEHEN_PROFILE_AUTH set to the private files.
Builders refuse to overwrite output directories. The package builder checks
that the embedded header exactly matches the tested blob and its manifest.
Generated code fingerprints contain hashes, not captured firmware instructions.

Validation performed:

- 2112 predicate/continuation checks, including expired-lease rejection, across
  normally linked trampolines and the exact resident blob mapped at 1 TiB.
- 19 transaction scenarios with eight host threads and full-buffer comparisons.
- Native hash normalization, exact-window conflicts and all six fingerprint
  range conflicts; a standard SHA-256 known-answer check.
- Eight app test suites, including 30 connection cases and mocked native journal
  ordering, normal cleanup, crash recovery, failed enable, target changes,
  pending-journal refusal, corrupt-journal refusal and boot-pointer invalidation.
- Existing manual build remains byte-identical to the accepted 8416-byte blob.
- Target compilation with warnings-as-errors, signature/profile checks and
  complete PKG validation.

Candidate (not yet hardware accepted):
`build/service-ui-v20-auto-ready/IV0000-BLHN00007_00-BLUEHENSERVICE01.pkg`,
6619136 bytes, SHA256
`fab09a3ea6fe6f84c3c77f85253fdd5ed2c4aa463390b6858b77b5cbe06184d7`.
Embedded automatic blob: 12528 bytes, SHA256
`6c8ec897c883cc80addd0a14735ac835d0873a6bc069ccf77bb17805103401b4`.

Hardware gate: verify AUTO_READY at manual launch, then a user-initiated
connection with AUTO_ENABLED and final AUTO_RESTORED. Independently read back
the original pages and inspect retained identity. Separately test a subsequent
app launch reusing the same resident. Crash-lease behavior is currently host
verified, not a physical console acceptance result. Do not run an old desktop
supervisor against this app or delete its journal to force another allocation.

## Physical startup result and 0.21 recovery

0.20 was installed with full package readback on 2026-10-31. Two manual
launches exited before SDL with AUTO_INIT_FAILED, initialized=0, result=10.
No automatic journal was created. Fresh Receiver-process calls verified that
sysctl({1,21}) and sysctlbyname("kern.boottime") return -1/ENOENT on this
console; TSC frequency was 1593749139 Hz and /dev/urandom opened successfully.
The Receiver probe is supporting evidence, not an execution of BlueHEN's
remaining startup steps. No automatic kernel hook was activated.

0.21 logs the boot-identity failure stage and keeps the menu available on
automatic initialization failure. It blocks connection requests before the
connection probe, while retaining discovery and normal exit. The existing
boot/journal safety requirements remain in force. Missing boot identity is
covered in the host controller test; all eight app suites and package build
checks passed. Automatic admission remains blocked pending a validated boot
identity source. Menu behavior still needs physical acceptance.

Installed/readback-verified candidate:
`build/service-ui-v21-startup-fix/IV0000-BLHN00007_00-BLUEHENSERVICE01.pkg`,
6619136 bytes, SHA256
`23f18737c42f9e5c3f1ae87dcac911f316ca8d31973eaf6e173e773f70ab5f1a`.
