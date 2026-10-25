# Daemon registration gate, firmware 9.60

The RegisterDaemon request reaches a ShellCore handler with an explicit
system-process authorization check. Its rejection returns **0x8094000f**, the
same error observed in BLHN00002 PID137. The check uses get_authinfo and
caps[0] bit62, also used by Mbus classification. PID137's own pre-call query
returned0 with this bit clear. This strongly identifies the cause; no live
instruction trace or service diagnostic for PID137 was captured.

## Command path

In local libSceSystemService, RegisterDaemon at0x4d70 jumps via0x5c00 to
0x9d40, then IPC stub0xd710 sends command0x30031. Its local uninitialized
branch returns0x80940001, a different error.

In SceShellCore:

| Stage | Firmware-relative offset | Operation |
| --- | --- | --- |
| Dispatcher | 0x12c00 | Subtract0x3000e, accept indices through0x3d |
| Relative jump table | 0xe1b0b8 | Index0x23 for command0x30031 |
| Command case | 0x12e9f | Call handler0x336600 |
| registerDaemon handler | 0x336600 | Obtain session PID via vtable+0x18 |
| Process predicate | 0x683af0 | Query get_authinfo, extract bit62 at output+8 |
| Decision | 0x33662d | Require predicate return exactly1 |
| Rejection | 0x33668a | Diagnostic and return0x8094000f |
| Allowed continuation | 0x336666 | Resolve app identity, then0x32c0b0 |
| Registration implementation | 0x3060d0 | Reached through0x32c0b0 after checks |

The rejection's local diagnostic describes a non-system caller, and a nearby
label names the handler registerDaemon. The category/SFO is not consulted in
this early predicate. Further conditions after the authorization gate have not
been exhaustively traced and must not be assumed satisfied by setting one bit.

Command numbers are local to an IPC interface: another ShellCore dispatcher
also handles the numerical range0x30000 but is unrelated. The case above is
linked to the identified registerDaemon handler, not selected by number alone.

## Live verification

Local SceShellCore ELF SHA-256:
`b3b352a4a302909dab7ba5533203be826d4b204f8dbd9bcaa8693ddf1bd61cff`.

The read-only diagnostic rediscovered SceShellCore, checked title NPXS20000,
firmware960 and the executable mapping. Six ranges matched the local image:
0x12c00/54 bytes, the four-byte table entry,0x12e9f/13,
0x336600/274,0x683af0/133 and0x32c0b0/98.
The predicate's import at PLT0xe16bf0 resolved to the matching get_authinfo
export in live libkernel_sys, including a matching code prefix.

No RPC calls, registration retries, credential changes, process launches or
code writes occurred in this verification. Selected-byte equality does not
constitute an execution trace or full-image equality.

Private local evidence: ../review/live-229/register-gate-live.json,
check_register_gate.py and register-daemon-metadata.json. Firmware and raw
analysis stay outside the repository. This investigation did not consume any
new launch ticket; ticket8 remains consumed.

## Consequence for the standalone design

Both known early gates require the same runtime capability bit. Another app
category, another calling thread or repeated initialization is not supported
as a fix by this evidence. The next discriminating prototype should isolate
how the *owned BlueHEN process* receives its authorization profile at launch,
then query the actual result before attempting registration.

The public standalone-daemon example already inspected uses custom fake-SELF
authorization metadata as well as a jailbreak helper and system-title placement.
Do not attribute its success to any one of those ingredients without testing.
Inspect the builder/loader's handling of that profile, preserve the current
negative-control package, and build a separate version for any experiment.
First record get_authinfo, registration and clean shutdown; audio tests follow
only if that lifecycle works. No ShellCore/Mbus service patch is needed to
investigate the owned process's launch configuration.

## 2026-10-25: physical 9.60 daemon registration succeeds (0.04)

After the user confirmed a freeze and rebooted, a fresh debugger session was
created and verified. Launch submission was separated from result consumption;
FTP app-log observation came first. Both launches completed without observed
connection loss. This does not establish the cause of the earlier freezes.

BLHN00004 0.03 ran as PID71: the complete 136-byte runtime auth profile matched
the private expected profile, including caps[0] bit62=1. Its initial inventory
contained only eboot, libkernel, libc and Sysmodule. SystemService was absent;
the app logged END5 and process exit was verified. Ticket0 was subsequently
consumed once (0x60000e0d).

BLHN00005 0.04 adds one process-local Sysmodule acquisition (internal id
0x80000010, OpenOrbis _types/sysmodule.h) after full auth-profile verification,
and releases that acquisition before exit. It preserves the firmware check,
fresh-PID permission gate, bounded heartbeat, and PID-specific stop request.
No live credential writes, audio or Bluetooth calls are performed.

Physical result PID74: profile match=1; SystemService load=0; RegisterDaemon=0;
heartbeats 0 through 6; STOP accepted; SystemService unload=0; END0. Debugger
confirmed the process had exited; control files were removed. Ticket1 was
consumed afterward (0x60000f0d). No other-title transition was tested, so continued
background execution across game launch remains unverified.

Exact tested package: `build/service-app-v4/IV0000-BLHN00005_00-BLUEHENSERVICE01.pkg`
(6,619,136 bytes), SHA-256
`2a20789949fc30223c69af678344b01e28a4c4024cd438f96e461047b6d98cc7`.
Compiler warnings-as-errors, package validation, signature/profile checks, four
host tests, installation and full installed-package readback passed. Receiver
PID68 accepted installation without a manual restart this boot (its first task).

Local evidence outside Git: `review/live-229/service-v3-reboot3.log`,
`service-v4-console.log`, `service-v4-run.json`, `service-app-v4-side-install.json`,
`consume-v3-reboot3.json`, `consume-v4-reboot3.json`, and `reboot3-session.json`.
Private profile/header/build inputs remain local and must not be committed.

Next: a separately gated, bounded Vmic create/destroy probe within this own
registered process, with before/after audiod slot checks and exact-handle cleanup.
Registration success does not prove audio access, PCM delivery, headset output,
microphone capture, Bluetooth pairing, or party/game routing.
