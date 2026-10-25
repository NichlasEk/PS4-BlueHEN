# Package cleanup and lifecycle on firmware9.60

## Reboot7: normal-category exit verified twice

00.09 replaced the hash-matched00.08 through fresh verified ShellUI RPC
(AppUnInstall0); Receiver installation and full package readback matched
592df88c3965c1f5bce3e0aaee08eb399cd85ce1e86b8f25d4eea39e33c7526e.
Only BLHN00007 is installed. A fresh ShellCore RPC submitted the launch.

PID71 matched its private profile, passed the PID-bound permission and stop gates,
and exited via LoadExec. The BGFT own-title list became empty. An independent
read of the six ProcessStarter slots found no own record and slot1 inactive.
After consuming ticket0 once, the exact installed package was launched again
without reboot or reinstall: PID75 repeated the same successful exit and both
independent state checks were clear. Ticket1 consumed once. No daemon registration,
audio or Bluetooth calls occurred. This proves repeated lifecycle for this gd
baseline; it does not prove background daemon behavior or headset support.

A fresh read-only app.db snapshot reports category=gd and canRemove=1 for
BLHN00007 in all four user browse tables. This is database evidence for menu
removability, not a user-observed menu deletion. Replacement AFTER these runs
has not yet been tested. Receiver stopped responding after foreground app use;
the user was asked to close/open Receiver, without rebooting the console.

Private evidence: reboot7-debugger-load.json, replace-v08-for-exit-v9-reboot7.json,
service-exit-v9-side-install.json, reboot7-shellcore-session.json,
service-exit-v9-run.json, service-exit-v9-repeat-run.json,
exit-v9-app-slots.json, exit-v9-repeat-app-slots.json,
normal-category-v9/browse-flags.json. Raw databases remain private.

Next candidate00.10 keeps gd/stable BLHN00007 and the same auth profile, with
NO daemon registration. It performs the bounded registered-info BT query from
the existing ABI research, logs no device names/addresses, closes its own fd,
and exits through LoadExec. No scan, pair, connect, disconnect or audio calls.
Build, signature and package validation passed; existing4 tests passed.
Artifact build/service-bt-v10/IV0000-BLHN00007_00-BLUEHENSERVICE01.pkg,
6619136 bytes, SHA256
678d7138a6dc355b5c1776c56945dc7a104d87c8021f073413fab37938b7f0ab.
NOT installed/run yet. Preserve exact accepted00.09 package. Before replacement
require Receiver readiness, empty own started list, no own process, and exact
installed00.09 hash; local replace_v09_for_bt_v10.py and the install/launch/run
normal_bt_v10 scripts are prepared for this boot's fresh verified sessions.

## Reboot6: 00.08 refused before registration; normal-category baseline ready

The user confirmed that the prior relaunch froze the console, then rebooted it.
Reboot6: pinned debugger loaded once, firmware9.60 verified, own started list
empty. Fresh ShellUI RPC removed the hash-matched00.07 with return0. Receiver
installed00.08 under BLHN00007; full readback matched6742d516...05a308e.

Physical PID71 matched its authorization profile and loaded SystemService, but
sceKernelDlsym("sceLncUtilUnregisterDaemon") returned0x80020003. The named NID
exists in the extracted module, but this does NOT imply application-side dlsym
accessibility. The test refused before its permission gate and RegisterDaemon,
unloaded SystemService successfully, logged END result8 and exited. Neither
unregister nor LoadExec ran. Do not claim that the unregister approach was tested.

Despite NO daemon registration, the own started marker and active application
slot1 remained (type2/state1). Thus RegisterDaemon is not required to trigger
this stale state. The generic descriptor vector no longer contained the title.
Launch ticket0 was consumed once:0x60000e0d. No relaunch/uninstall retry followed.
Private journals: reboot6-debugger-load.json, replace-v07-for-exit-v8-reboot6.json,
service-exit-v8-side-install.json, reboot6-shellcore-session.json,
service-exit-v8-run.json, service-exit-v8-console.log, exit-v8-app-slots.json,
exit-v8-descriptors.json, launch-BLHN00007-system-exit-v8.json.

Static ProcessTerm path0x3de460 is called from unmountApp0Dir0x314700. Relevant
cleanup branches at0x3dfc4c and0x3e0b4e require descriptor types4/5. This suggests
the installed gdd launch category is incompatible with the normal package
cleanup path; it remains an inference, not a captured branch trace.

Prepared00.09 isolates normal-app category gd with the same stable title and
private authorization profile. It performs NO daemon registration, audio or BT;
it checks its profile, waits for a PID-bound gate/stop, then LoadExec("exit",NULL).
Build, fake-SELF verification, package validation and existing4 tests pass.
Artifact build/service-exit-v9/IV0000-BLHN00007_00-BLUEHENSERVICE01.pkg,
6619136 bytes, SHA256
592df88c3965c1f5bce3e0aaee08eb399cd85ce1e86b8f25d4eea39e33c7526e.
NOT installed or physically tested. User asked for a normal reboot, GoldHEN and
Receiver before replacement, because relaunching the stale state caused a freeze.
Local v9 harness explicitly refuses launch if any own started marker remains.
No additional title IDs have been installed. After a fresh boot, re-establish
all sessions and verify installed00.08 before replacement; do not reuse reboot6
RPCs or tickets. Observe process exit, slot release and BGFT release separately.

## Follow-up: live slot evidence and unregister candidate

A bounded read of the six ShellCore application records confirmed that slot1
still contained BLHN00007 with active=1, head_type=2, BGFT type=2 (field0xa858),
and state=1 after its process exited. The stop-suppression counter was0. This
establishes the type from the live own-title record, but also shows that clearing
only the BGFT list would leave other application state behind.
Private journal: exit-v7-app-slots.json, reader read_app_slots.py.

GetAppTitleId queried with the saved launch result0x60000e0d returned0x80940004
and an empty title. No KillApp call was made with that unresolvable app ID.
The earlier GetAppId result0xffffffff therefore must not be interpreted as proof
that all ShellCore application bookkeeping is gone.

One attempt to relaunch the SAME installed0.07 was submitted successfully with
ticket1. Its previous console log was verified against the preserved local log
and renamed to service-exit-v7-first-run.log. No new log was found. Subsequently
connections to debugger744, FTP2121 and Receiver12801 were refused/unavailable.
The result-consumption script failed at initial socket connection, BEFORE any
RPC: ticket1 has NOT been consumed. This does not establish whether the console
froze, rebooted or lost services; the user was asked for its visible state.
Do not resubmit the launch or reload the debugger into this uncertain boot.
Private journal: launch-BLHN00007-system-exit-v7-unregister.json.

Static analysis found the exported sceLncUtilUnregisterDaemon at0x5c10 in the
local9.60 SystemService module. It takes NO arguments and sends IPC0x30032 with
no request data; server identity determines the calling app. Its manager path
unregisters daemon events. This is not evidence that it cleans package slots.
Never invoke it in ShellCore/ShellUI to unregister a different application.

Prepared lifecycle-only00.08 adds own-process unregister after a successful
registration, including timeout cleanup, before the existing LoadExec exit.
It refuses to register if the unregister export is unresolved. Package/signature
validation and compilation with warnings-as-errors passed. Stable titleBLHN00007,
6619136 bytes, SHA256:
6742d5163b663876751ce601cae6f90d7c39c0e939bc7c6a50b266f2b05a308e.
Artifact: build/service-exit-v8/IV0000-BLHN00007_00-BLUEHENSERVICE01.pkg.
NOT installed or physically tested. Preserve0.06 and0.07 artifacts unchanged.

Next physical gate: establish console/boot state, rediscover debugger identity
and all sessions, verify own-title process/slot/started-list state and installed
package hash before replacement. Adapt the run harness to fingerprint the
unregister export in the TEST PROCESS before granting its PID gate. Observe
unregister return, process exit, application slot and BGFT list independently.
Do not claim success from an unregister return value alone.

## Stable-title exit experiment (current state)

After cleanup, the preserved accepted0.06 package was hash-checked on the host.
BLHN00007 was uninstalled through the verified ShellUI API and replaced with an
isolated lifecycle-only00.07 build under the SAME title ID. Full installed-package
readback matched SHA256
300169e7f68cf474ab725b9cd1327794f78b0c0569112f0e1f38e16b2e218717
(6619136 bytes). This is build/service-exit-v7, NOT the older Bluetooth query
candidate in build/service-app-v7. The latter uses BLHN00008 and must not be
installed. Use a later version for subsequent stable-title Bluetooth builds.

Physical PID71 matched the expected private authorization profile, registered
as a daemon successfully, emitted heartbeats, accepted its PID-bound stop file,
and logged EXIT_SYSTEM_BEGIN before calling sceSystemServiceLoadExec("exit",NULL).
The process disappeared without logging a return or fallback. GetAppId then
returned0xffffffff, but the BGFT started-title list still contained BLHN00007.
A later independent read-only check confirmed the same marker and responsive
ShellCore41/ShellUI42. Thus LoadExec alone DOES NOT solve repeated replacement.
No further launch, uninstall retry, reboot or internal-list edit was performed.

Private evidence: replace-v06-for-exit-v7.json, service-exit-v7-side-install.json,
launch-BLHN00007-system-exit-v7.json, service-exit-v7-run.json,
service-exit-v7-console.log, exit-v7-appid.json, exit-v7-readonly-recheck.json.
The launch ticket0 has already been consumed; do not consume it again.
The fresh reboot5 ShellCore RPC is recorded in reboot5-shellcore-session.json;
previous boot sessions remain invalid. Receiver has handled this installation;
its ability to accept another registration must be re-established before use.

### Stop notification investigation (static evidence, not a tested fix)

ShellCore's internal GameStopped at virtual0x9e5cb0 accepts a title pointer and
an integer type. It calls the started-list removal helper0xa60f20 and then
0xa2c380. The normal exit caller at0x3e0122 obtains the type from app-object
field0xa858. This is not a public export recovered from libSceBgft.

Two assignment paths were traced: the path storing at0x3dd1fc selects3 normally
or4 for /hostapp/app and /data/app path prefixes. The other path stores at
0x3dac97, with an upstream /preinst2/app comparison selecting6, otherwise a
separate discriminator selecting2 or7. These are distinct paths; the type for
our particular gdd launch has NOT yet been established. In particular, choosing6
merely because the downstream method fast-returns would be unjustified.

Next: establish which launch/teardown path owns this gdd app, its exact type and
thread requirements, and whether a normal supported stop notification can be
used. Do not call internal GameStopped with guessed parameters or mutate the
started-title vector. A successful process exit is not package-manager teardown.

## Cleanup completed after user reboot (before exit experiment)

In reboot5, a fresh debugger session and ShellUI PID42 were verified. The last
old title BLHN00006 matched its expected package hash, had no running process,
and AppExists returned true. AppUnInstall returned0 from the verified ShellUI
context. No database edits or manual console-file deletion were used.

Independent FTP and read-only database snapshots confirm that only BLHN00007
remains in /user/app, /user/appmeta, tbl_appinfo and all four user browse tables.
Its full package SHA256 still matches the accepted0.06 artifact:
5bc38620b0b9907dd13ec30cd3ddb1b0d407c456cdc88920f1e75fb7bb3b83e9.
All six old test titles are removed. The reboot-assisted recovery is now
physically verified; automatic lifecycle teardown without reboot still needs a
fix and physical testing. At that point no0.07 package had been installed; see the later experiment above.

Private journals: cleanup-last-reboot5.json and
cleanup-reboot5-final/verification.json. Older RPC sessions/tickets are invalid;
new ShellUI instrumentation is recorded in the reboot5 cleanup journal.

## Earlier investigation

## Verified cleanup, 2026-10-25

Seven BlueHEN titles accumulated during separate experiments. The user reported
that daemon-category packages could not be removed through the normal app menu.
A read-only copy of app.db showed BLHN00001 category=gd/canRemove=1, and
BLHN00002..00007 category=gdd/canRemove=0 in all four user browse tables.
This explains the menu restriction; it does not establish API removal failure.

Removal through the already loaded libSceAppInstUtil in ShellUI succeeded for
BLHN00001..00005. For each title the installed package hash matched its preserved
local artifact, no corresponding test process was running, fresh ShellUI identity
and export code were verified, own getpid and system-bit query were checked,
AppExists returned true, and AppUnInstall returned0. No live database editing,
manual file deletion or credential mutation was used. The runtime RPC scratch
allocation/thread is instrumentation, not a claim of zero process mutation.

A final independent FTP/database snapshot confirmed these five titles absent
from /user/app, /user/appmeta, tbl_appinfo and all four tbl_appbrowse tables.
BLHN00007 (tested0.06) remains intact with package SHA256
5bc38620b0b9907dd13ec30cd3ddb1b0d407c456cdc88920f1e75fb7bb3b83e9.
BLHN00006 (old0.05) is still installed. Its uninstall returned0x80990007;
a bounded retry after re-observation returned0x8099008b. No further retries.

AppExists writes a boolean byte on this firmware. An initial scratch value-1
was observed as0xffffff01; the assertion stopped before any uninstall. A fresh
zeroed output produced1. Do not assume that every header's int32 output type
matches the byte width actually written.

## Why the last old title is still blocked

Static ShellCore analysis shows0x8099008b returned when helper0xa611a0 finds a
title in a global list. Its object pointer is at virtual0x15294e0; a bounded
live read, after code-prefix checks, found BLHN00006 and BLHN00007 in that list.
The insertion path0xa60d90 is called from the BGFT GameWillStart path0x9e5b20.
Therefore process exit and Vmic resource cleanup do not establish full application
lifecycle teardown: the package manager can still regard the title as started.
No internal list entries were changed. A user reboot was requested, followed by
rechecking this state and uninstalling ONLY BLHN00006, preserving BLHN00007.
The proposed reboot recovery has not yet been validated.

The earlier0x80990007 failures are not conclusively explained: ShellCore contains
system-authorization checks returning that code, but the last ShellUI attempt had
system_bit1. Do not claim category alone or caller context alone was the cause.

## Development policy after cleanup

Do not install prepared BLHN00008 or create more side-by-side test titles.
Retain BLHN00007 as the stable development title. Preserve the exact hardware-
accepted package on the host, then use an explicitly journaled replacement flow
for subsequent builds. Use a later version for that title and adapt installation/launch scripts before
resuming Bluetooth work. The one replacement before launch succeeded; repeated
replacement after daemon exit remains unverified.

Fix and physically verify lifecycle exit before relying on repeated updates.
Candidate APIs found in the local SystemService exports include LoadExec,
LncUtilKillApp, GetAppId and GetAppTitleId. Their discovery is not authorization
to call them with guessed appids or ABIs; verify exact live title ownership and
recover the relevant ABI first. Keep the own-process resource cleanup intact.

Local evidence (outside Git): cleanup-old-service-first.json (preflight stop),
cleanup-old-service-verified.json, cleanup-BLHN00001/3/4/5/6.json,
cleanup-BLHN00006-reobserved.json, cleanup-uninstall-list.json and
cleanup-verified-five/verification.json under review/live-229. Raw app.db/WAL
snapshots and firmware disassembly are private and must not be committed.
