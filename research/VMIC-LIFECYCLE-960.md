# One-shot Vmic creation and cleanup, 2026-10-25

The console test observed a virtual microphone being allocated and then
freed, but **the API lifecycle did not pass**: both Create and Destroy returned
`0x809b00ff`. No microphone PCM, Bluetooth transport or chat routing was tested.

## Main-thread comparison: version 0.03

On 2026-10-25 Receiver PID125 installed version00.03 after removal of the
hash-verified previous test package. Full installed-package FTP readback
matched SHA-256
`218f922b5f10e94ffedfbe2847d6d45a7d64d202010f5ab18028d269af622acf`.
The new test process was PID128, title BLHN00001. Its resolved Create/Destroy
addresses and full function bodies matched local firmware before permission.
No debugger RPC thread or scratch allocation was installed in that process;
the app called both functions on its ordinary main thread. ShellCore RPC was
used separately for installation/launch.

Create(0,128,16000,1) returned 0x809b00ff. Slot0 became active, owner128,
handle0x40200, 128 frames, 16000 Hz, mono. The supervisor rechecked ownership
and supplied that exact handle. Destroy(0x40200) also returned 0x809b00ff,
but slot0 became inactive with owner0. All four slots were inactive before
the test, after Destroy and after process exit. END result=4 was logged,
process exit observed, and both control files removed. No PCM was read or
written and no routing or Bluetooth operation occurred.

This reproduces the failure without a debugger RPC thread in the caller,
so that thread alone cannot explain it. It does not prove the precise Mbus
service rejection branch was taken. Next, observe the Mbus client registration
and classification during a gated ordinary-main-thread run and compare its
requirements with legitimate system clients. Do not suppress the return code
or change service permission checks to claim success.

Private evidence: module-app-v3-install.json, vmic-main-v3-run.json,
vmic-main-v3-console.log, launch-BLHN00001-vmic-main-v3.json under
../review/live-229/. Launch ticket6 has been consumed to its final result;
never consume it again or rerun the one-shot scripts against these journals.

## Version 0.02 RPC method

The unchanged installed BLHN00001 version 0.02 package was verified by full
FTP readback (SHA-256
`6d93b310fa88900838c5c8370e4e538a94c1f9abaad1583cb53dde93f890a8ce`).
A new process, PID119, was verified by title and identity. During its bounded
gate wait, a debugger RPC thread/scratch allocation was installed in this
test process. Firmware, getpid, fresh module inventory and Dlsym results
were checked. Complete Create/Destroy symbol bodies (309/222 bytes) matched
the local 9.60 AudioIn module before invocation.

The tested register arguments to Create were `(0, 128, 16000, 1)`. The
examined wrapper ignores its incoming first argument; this is a firmware-
specific experiment, not a general declaration of the public ABI.
Create was invoked exactly once. `vmic_lifecycle.py` managed observations
and cleanup; it is orchestration code, not part of the installed app binary.

## Observed result

| Stage | API return | Observed daemon slot 0 |
| --- | --- | --- |
| Before | — | inactive, owner PID 0 |
| Create | 0x809b00ff | active, owner PID119, handle 0x00040100, 128 frames, 16000 Hz, 1 channel |
| Destroy(0x00040100) | 0x809b00ff | inactive, owner PID 0 |
| After process exit | — | inactive, owner PID 0 |

The error concealed the handle from the Create return value. Cleanup used
the exact handle read from a newly allocated slot owned by PID119, with
ownership rechecked immediately before Destroy. No guessed or foreign handle
was used. One Destroy call was made; no automatic retry occurred.
All four slots were inactive after cleanup and after process exit. The stale
handle/format fields remained, as expected for this slot layout. This verifies
active/owner cleanup, not every possible service-side resource.

The original app subsequently reached its existing-module refusal (result8)
and exited. That app log is separate from the lifecycle RPC journal. Its
gate was removed. Launch ticket4 has had its final result retrieved
(0x6000230d); do not consume it again.

## Interpretation and next work

The allocation/free observations match the previously identified failure
path after the daemon operation. The AudioIn wrappers then notify Mbus through
NID i1D0nTVcCgk. That export invokes an IPC stub at Mbus offset0x25a0, command
0xce110040. The same failure on Create and Destroy is consistent with this
notification failing. This is an inference from code and observations;
the precise service-side cause and permission requirements remain unresolved.

Do not treat negative Create as proof that nothing was allocated. Do not
hide the Mbus error, advertise a working microphone, or patch service
permissions to force success. Next, trace that notification's requirements
and distinguish normal registration from the debugger-thread context.
VmicWrite and PCM delivery remain untested.

## Evidence and verification

Private evidence is outside Git under `../review/live-229/`:
`vmic-create-run.json`, `vmic-create-console.log`, and
`launch-BLHN00001-vmic-create.json`. The raw journal includes a reporting
correction: the first helper incorrectly marked an error return as a matching
handle. The raw API returns/slot observations were retained; the corrected
field is false. Lookup-phase `called=false` fields predate the later lifecycle
calls; the lifecycle event list is authoritative for those calls.

Five host tests exercise ownership refusal, successful cleanup, cleanup when
Create conceals an allocated handle behind an error, foreign ownership, and
no automatic retry or speculative cleanup after transport timeout:

```
python3 -B -m unittest discover -s research -p test_vmic_lifecycle.py -v
```

## 2026-10-25: 0.05 physical lifecycle passes

After the confirmed hard freeze and user reboot, a fresh debugger session was
verified (reboot4 ShellCore PID42), and Receiver PID69 installed BLHN00006
without a title cycle. Installed package readback matched the preserved 0.05
SHA-256 `60ddc2b43cd25e8603d258e849e0d5fda8b9a9b024d3ab2d9c91c1be284d794f`.
Own process PID72: complete auth-profile match, RegisterDaemon=0, AudioIn load=0,
VmicCreate(0,128,16000,1)=0x40100. Audiod slot0 was active and owned by PID72,
128 frames / 16kHz / mono. Exact-handle Destroy returned0; the slot was inactive
with owner0 before process exit and remained clear afterward. AudioIn and
SystemService unload returned0, END0, exit and removal of control files verified.
No PCM write, routing or Bluetooth call was made. This establishes lifecycle
success, not headset capture or delivery to game/party chat.
Launch ticket0 consumed once afterward: 0x60000e0d.
Local evidence: service-v5-run.json, service-v5-console.log,
service-app-v5-side-install.json, consume-v5-reboot4.json, reboot4-session.json.
