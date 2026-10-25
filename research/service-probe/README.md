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

**No version0.02 package exists yet.** Capturing the baseline from the unchanged
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
