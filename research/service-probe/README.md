# Service lifecycle experiment v0.01

This is a separate title, BLHN00002, with no audio/Bluetooth operations and
no credential changes. It is not yet a verified background service.

The source borrows existing libkernel and SystemService exports. It records
PID, parent PID and the result of get_authinfo, retaining only caps[0] bit62.
It then waits up to 60 runnable seconds for a PID-specific permission file,
calls sceSystemServiceRegisterDaemon once, and aborts if that call fails.
On success it emits one heartbeat per second for at most 60 iterations, or
exits when a stop file contains its own PID. Suspension can extend the wall
clock duration: a sleeping/suspended process cannot enforce a real-time lease.
An external observer must verify END and process disappearance.

Paths under /data/bluehen/: service-v1.log, service-v1.allow, service-v1.stop.
No permanent installation hook or startup configuration is created by the app.
The app requires the existing diagnostic directory to be writable.

## Why this experiment

The local console's NPXS21007 metadata reports category gdd. The public
Itemzflow loader installs its separate daemon as a system title using that
system title's SFO as a template; its daemon calls RegisterDaemon after its
own jailbreak initialization. Its build also uses custom authorization data.
These are independent differences from our ordinary app. Merely selecting
gdd must not be reported as granting system credentials or background life.

This first package changes category to gdd but keeps our normal test paid
and its own generated SFO/title. It neither copies the system SFO into the
package nor changes /system. Normal package installation of this category,
registration, launch and background execution all remain experimental.
If rejected, preserve the exact error and inspect the launcher/category
requirements before another attempt. Do not silently move it into /system,
change its authorization profile, or switch category to claim success.

## Artifact

Built and package-validated on 2026-10-25:

- build/service-app-v1/IV0000-BLHN00002_00-BLUEHENSERVICE01.pkg
- 6619136 bytes
- SHA-256: 4ec77bb05d05b27d32ebf759460c16357dca12fdbec2b1f178d45f4e51f05b6b
- FTP staging and installed-package full readback matched. See physical result below.

Build from the repository root with:

```
python3 -B research/service-probe/build_pkg.py
```

Keep this artifact unchanged during testing. Runtime modules remain local.

## Physical result, 2026-10-25

Fresh Receiver PID134 installed the exact package above; installed app.pkg
readback matched. Launch ticket8 completed with app ID0x6000290d. The process
was BLHN00002 PID137, parent PID38 (SceSysCore). Runtime get_authinfo returned0
with caps[0] bit62 clear. Resolved RegisterDaemon/get_authinfo pointers and
code prefixes matched the local firmware before the PID gate was supplied.

One main-thread RegisterDaemon returned **0x8094000f**. The app logged
END result=2 and exited; both control files were absent/removed afterward.
No heartbeat loop, second-title launch, explicit stop acceptance or background
survival was tested because registration failed. Installation of category gdd
and a separate PID are verified, but a working daemon is not.

Offline inspection of this console's libSceSystemService shows the export at
0x4d70 jumping through0x5c00 to0x9d40. That wrapper returns0x80940001 when its
initialization count is nonpositive. Otherwise it reaches the IPC stub0xd710,
which sends command0x30031 and propagates transport or service result.
Our observed0x8094000f therefore differs from that local initialization failure;
its precise transport/service rejection remains unresolved. The body analysis
is offline; only the resolved export prefix was compared in this run.

Command0x30031 has now been traced and its gate verified against live code;
see [DAEMON-REGISTER-960.md](../DAEMON-REGISTER-960.md). Do not infer that
gdd metadata grants runtime capabilities, repeat the same call, or add microphone
calls before registration has an understood execution context.

Private evidence: ../review/live-229/service-app-v1-install.json,
service-v1-run.json, service-v1-console.log,
launch-BLHN00002-service-v1.json and register-daemon-metadata.json.
Ticket8 is fully consumed and must not be consumed again.

## Console acceptance sequence

1. Fresh Receiver identity/session, one install request, installed-package hash.
2. Fresh BLHN00002 process, exact live get_authinfo and RegisterDaemon exports,
   no stale log/control files; only then supply its PID to the allow file.
3. Record registration result and observed runtime authorization independently.
4. If registration succeeds, launch another test title and observe continuing
   heartbeats; distinguish an accepted launch ticket from actual foreground
   display and from a process that is merely present but suspended.
5. Supply exact PID to stop, observe END and process exit, remove control files.
   If it is suspended, restore its execution context before judging stop.

A separate PID is necessary but does not prove background operation, survival
of a future launcher exit, microphone permission, rest-mode behavior or
Bluetooth support. No claims of those capabilities are made by this prototype.

Sources examined:
[Itemzflow loader](https://github.com/LightningMods/Itemzflow/blob/main/itemz-loader/source/daemon.cpp),
[daemon entrypoint](https://github.com/LightningMods/Itemzflow/blob/main/itemz-daemon/source/main.cpp),
[daemon build](https://github.com/LightningMods/Itemzflow/blob/main/itemz-daemon/Makefile).

## Version 0.02 preparation: one-bit launch profile

`main_auth.c` and `build_auth_pkg.py` prepare a separate version00.02 artifact.
The builder requires private `BLUEHEN_BASELINE_AUTH` and `BLUEHEN_PROFILE_AUTH`
files of136 bytes. It rejects a profile unless only caps[0] bit62 differs,
preserves the measured runtime paid, embeds the profile through create-fself's
`--authinfo`, and checks the exact signature bytes in the output. The CLI paid
replaces the first profile qword in this builder, so it must match the measured
profile; it must not silently reuse the original package's paid argument.

The app compares its actual get_authinfo result against the requested profile
before enabling registration. A mismatch exits with result4. This strict check
may reject dynamic fields even if the system bit is present; retain evidence
and examine differences privately before weakening it. The experiment makes
no in-process credential writes and does not patch a system service.

Four host tests pass for target-bit validation, unrelated changes, size/already-
system refusal and missing/duplicate/truncated signature records. The PS4-target
source compiled using a synthetic all-zero header strictly for compile checking;
that object is not a valid deployable test artifact.

**Initial attempt (superseded by the reboot-session result below):** Capturing the baseline from the unchanged
v0.01 app was interrupted during launch. Ticket9 had returned EAGAIN three times;
the next debugger mapping read timed out before the next result RPC submission.
Subsequent debugger744 and FTP2121 connections were refused. No baseline/profile
blob was captured, no registration gate issued, and no new profile installed.
The console state and launch outcome are unknown; the user was asked what the
screen shows. Do not resubmit the launch or invent a baseline from another app.

The prior service-v1.log was verified and archived as service-v1-first.log
before this attempt. New local journals: service-auth-baseline.json and
launch-BLHN00002-service-auth-baseline.json. If the boot/session survives, inspect
ticket9 and process/log state before any continuation; after reboot rediscover
all mappings/RPC state instead of reusing old addresses or tickets.


## Reboot-session profile experiment, 2026-10-25

The user confirmed a reboot. The pinned debugger was sent once and verified
on9.60 after startup. A new ShellCore RPC session was installed and verified;
no old stub, scratch address or ticket was reused. The unchanged BLHN00002
v0.01 ran as PID71. Its136-byte get_authinfo result was captured privately,
and a second profile differs in exactly caps[0] bit62. No registration gate
was issued. The app logged END result=1 on permission timeout and exited.

Baseline SHA-256:6002c4a17b06cd6b00b1837c8e762c959a848a99ca1e02e6ed8f20cc73a2b93a.
Requested profile SHA-256:a969162168ef1bb7f5912289f993a7fa614043ed2468b6e0098e45e9011ef5e2.
Profile files, generated headers and fake-SELF material remain local.

Replacing BLHN00002 was refused by AppInstUtil with0x80990007. Readback proved
the original package remained unchanged. Instead, the profile experiment was
packaged as separate title BLHN00003 v00.02. Installation and full readback
matched c7b0dc282ee5047cdc0f6d3c92efbd326deef3b509481785c226704b50591f3e.
The older BLHN00002 v00.02 artifact in the build directory was never installed.

BLHN00003 launched as PID74 and logged unresolved lifecycle exports, then
END result=1. A separate process query verified its exit. Since the code
required all lifecycle exports before querying authinfo, no runtime profile
measurement occurred. No registration call or heartbeat/stop test ran.
Do not infer that the loader accepted or rejected the profile from this result.

Version00.03, BLHN00004, fixes that diagnostic ordering. It inventories modules,
logs each matching export lookup, accepts known libkernel module-name variants,
and measures authinfo before requiring the daemon export. It retains strict
profile comparison before registration; no modules are loaded by this change.
Build/package/profile validation passed, and FTP staging/readback matched:

- build/service-app-v3/IV0000-BLHN00004_00-BLUEHENSERVICE01.pkg
- SHA-256:f98747e30c324169da6836ad1e0119b5f5bb2a5ca6ba207023fc1498ce49d838
- 6619136 bytes; subsequently installed/readback-verified; launch outcome unresolved below.

Build with build_auth_diag_pkg.py and the same two private profile-file
variables. Four profile host tests pass. Package validation does not prove
runtime authorization, background operation or microphone support.

Private evidence: service-auth-reboot2.json, service-auth-reboot2-console.log,
service-app-v2-install.json (refused replacement), service-app-v2-side-install.json,
service-v2-run.json, service-v2-console.log, service-v2-postcheck.json,
service-v3-transfer.json. New-boot launch tickets0 and1 are consumed; pre-reboot
ticket9 is obsolete. Preserve separate boot/session identities.


## Latest launch interruption and UT99 workflow correction

Version0.03 BLHN00004 was installed from a fresh Receiver session, and its
installed package matched the hash above. Launch ticket2 was accepted and
returned EAGAIN twice. A subsequent debugger operation timed out; fresh
connections to both744 and2121 were then refused. No service-v3.log could be
retrieved, so execution, authorization and cleanup are unverified. Do not
resubmit the launch. The user was asked about the console display/state.

The user's UT99 project documents successful repeated installations using the
same Receiver backend. Its HANDOFF-2026-10-21-PHYSICAL-SIGNALS.md and local
start-receiver.py, launch-title.py, consume.py and install-probe.py establish:

- One owned BGFT task per Receiver lifetime is a backend guard, not a general
  limitation requiring manual interaction between packages.
- Switching between the test application and Receiver refreshes the Receiver
  process as part of the automated iteration workflow. Verify a new identity
  and bgft_task=-1; merely bringing an existing process forward is insufficient.
- UT99 separates launch submission from result consumption. Our combined helper
  polls every0.5 seconds, including debugger map queries during app startup.
  This is a concrete workflow difference, not proof of the interruption's cause.

Future runs should submit once, observe startup through the app's unique log,
then consume that same ticket separately after fresh session/code checks.
No final ticket may be consumed twice. No launch may be repeated merely because
its result is unknown. The local launch_service_separated.py is prepared and
syntax-checked but unrun; it requires an explicit fresh RPC-session journal.
Do not point the old combined supervisor at it unchanged: that supervisor's
launch-success contract requires adaptation to pending results.

Private evidence: service-app-v3-side-install.json, service-v3-run.json and
launch-BLHN00004-service-auth-v3.json. The earlier reboot-session baseline
measurement remains valid evidence; the latest launch is not a profile pass.

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
