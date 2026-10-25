# Mbus notification gate on firmware 9.60

## Finding

The service handling the post-Vmic notification is in **SceSysCore**, not
the audio daemon. Its `NotifyDriverEvent` path has a system-process check
that returns exactly **0x809b00ff**. This is a concrete explanation candidate
for the previous allocation/free success followed by API failure.
It is not yet a trace proving which branch the earlier call took.

The examined SceSysCore ELF was retrieved from the user's console and stays
outside Git. SHA-256:
`812a75eaa7469865ab89904ab63c4ce77dcf3a7c52fa1500df99b6905f399cf5`.
Live SceSysCore identity was NPXS20991, PID38 at observation time. Do not
reuse this PID or any address without fresh identity/mapping checks.

## Traced path

- AudioIn calls Mbus export NID `i1D0nTVcCgk` after the daemon operation.
- That export at Mbus offset0x5790 obtains the caller's **PID** through
  `getpid` (NID `HoLVWNanBBc`), not a thread ID, and passes it to the IPC stub.
- Mbus offset0x25a0 serializes command **0xce110040**, with a 40-byte message.
- SceSysCore dispatcher0x80d40 subtracts the 0xce110000 command base and uses
  a relative jump table at0xb3ba8. Index0x40 targets0x816ac, then0x7d9c0.
- The handler checks its client session against a 32-entry service table.
  Entries are 24 bytes; session pointer is at+0, PID at+16, and the
  system-process classification byte at+20.
- Client registration at0x7ed02 fills those fields. Classification calls
  the process-based helper0x8c4d0. The helper examines process information
  returned by a kernel import; that import's full ABI/name remains unresolved.
- A separate exception path checks `sceKernelIsKratos` and another
  process predicate at0x8c710. It is not a general ordinary-app fallback.
- Rejection at0x7da67 emits a diagnostic saying the caller is not a system
  process, and returns0x809b00ff at0x7da8c. This occurs before downstream
  notification logic at0x53330. Vmic session setup at0x52980 is also downstream.

These observations make "just use a different thread" an insufficiently
supported fix. They do not justify changing another process's classification
or patching the service. No such changes were performed.

## Live verification and limits

Four code ranges matched the console: dispatcher0x80d40 (96 bytes), command
case0x816ac (39), handler0x7d9c0 (224), and exception predicate0x8c710 (128).
Registration0x7ed02 (84) and classification0x8c4d0 (128) also matched during
the follow-up read. This is selected code equality, not full-image equality.

A fresh isolated BLHN00001 process, PID122, ran a bounded diagnostic through
its own debugger RPC thread. `sceMbusInit` returned0, and
`sceKernelIsKratos` returned0. No Vmic Create/Write/Destroy call was made.
The attempted session-table lookup did **not** find exactly one row owned by
the test PID, and the diagnostic stopped with an assertion. The subsequent
table read showed only other active clients. It does not prove the test's
classification byte was zero, or which transport/session handled its call.

Afterward PID122 was absent and all four virtual-mic slots remained inactive.
The app log contained only READY, without a terminal END marker. Therefore
process disappearance is observed, but orderly application completion is
not established. Do not silently call this a successful classification test.
The gate file was never issued. Ticket5 final result was retrieved; do not
consume it again.

## Ordinary-main-thread observation, 2026-10-25

The unchanged, hash-verified version00.03 package was launched as fresh
BLHN00001 PID131. No RPC was installed in the test process. The supervisor
verified the service identity, current mappings, local ELF hash and live
handler/registration/classification code before reading the client table.
Each observation used two equal 768-byte table reads; these are snapshots,
not an atomic trace of the service handler.

| Observation | Test process in Mbus client table |
| --- | --- |
| Before Create, app waiting at gate | No row |
| After Create returned 0x809b00ff | Row10, owner131, system flag0, other flags1/0 |
| After Destroy returned 0x809b00ff | Same row and classification |
| After logged END and process exit | No row |

Ten pre-existing clients had system flag1 throughout these observations.
Create allocated Vmic slot0 to PID131 with handle0x40300, 128 frames,
16000 Hz and one channel. Ownership was checked again before supplying
that handle to the app's single main-thread Destroy. All four Vmic slots
were inactive after Destroy and after process exit. END result=4 was logged;
both control files were removed. No PCM, routing, Bluetooth or service writes
were performed. Ticket7 reached final result0x6000270d and is consumed.

This establishes the previously missing classification observation: the
ordinary app does register, but its observed system-process flag is zero.
The earlier lack of a row after MbusInit alone does not establish a failed
registration; registration was observable after Create in this experiment.
The zero flag plus matching handler code strongly supports rejection at the
system-process gate. No instruction trace or service diagnostic was captured,
so the exact executed rejection branch remains an inference.

## Next work

Repeated Create/Destroy experiments now have little diagnostic value.
Identify the process-information import used by classification and audit the
supported native service/plugin execution context before selecting an
integration design. A normal homebrew app has not demonstrated permission to
send this driver notification. A broker/service design would need explicit
resource ownership, process-lifetime cleanup and compatibility with GoldHEN;
its feasibility is not yet established. Do not disguise the ordinary app as a
system process or patch the service flag as a substitute for that design.
Bluetooth headset transport remains a separate unresolved requirement.

New private evidence: vmic-main-v3-mbus-run.json,
vmic-main-v3-mbus-console.log and launch-BLHN00001-vmic-main-v3-mbus.json.
The original v3 console log was archived as vmic-main-v3-first.log on console;
the current vmic-main-v3.log belongs to PID131.

Raw local evidence under `../review/live-229/`: mbus-service-code-checks.json,
mbus-classification-run.json, mbus-classification-observation.json,
mbus-classification-postcheck.json, mbus-classification-final-console.log,
and launch-BLHN00001-mbus-classification.json. No firmware, disassembly,
credentials or runtime addresses are included in the repository.
