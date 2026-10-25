# Package cleanup and lifecycle on firmware9.60

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
for subsequent builds. Rebuild0.07 for that title and adapt installation/launch
scripts before resuming Bluetooth work. Do not mark replacement support as tested.

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
