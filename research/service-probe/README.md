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
- FTP staging and full readback matched; not yet installed or run.

Build from the repository root with:

```
python3 -B research/service-probe/build_pkg.py
```

Keep this artifact unchanged during testing. Runtime modules remain local.

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
