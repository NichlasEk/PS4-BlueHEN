# BlueHEN service direction, 2026-10-25

## Scope

The user explicitly clarified that GoldHEN integration is optional. The target
remains the PS4's built-in Bluetooth controller, generic headset output and
microphone input. Prefer an independently owned BlueHEN service with a small
launcher; keep any GoldHEN integration in an optional adapter. This is a design
direction, not proof that a standalone privileged service can yet be launched.
A jailbreak/bootstrap requirement is distinct from requiring a GoldHEN plugin.

## Authorization check identified and verified

The previously unnamed import NID `igMefp4SAv0` is `get_authinfo`:
computing the Orbis NID from that name matches, and the console's extracted
libkernel_sys exports it at virtual offset0x2680, size24. Its wrapper invokes
syscall587 (0x24b). Local ELF SHA-256:
`310ed363acc520fed8fec48cfe432e6236fe75bba424cd3336ab057c7b70e392`.

A live read verified that SceSysCore's PLT entry at0x91440 resolves to the
matching libkernel_sys export, including its code prefix. The classification
helper at0x8c4d0 tests bit62 in the qword at output offset8 after a successful
query. Public shadPS4 source names that qword caps[0] in AuthInfoData, whose
first qword is paid. Its implementation is an emulator stub: use it as an ABI
cross-reference, never as physical authorization evidence.

A bounded console diagnostic invoked only getpid/get_authinfo through the
already-existing verified ShellCore RPC session. It changed only that session's
scratch output buffer, then cleared it; it did not change credentials, load
modules, create processes or call audio functions. All queries returned0:

| Queried process | caps[0] bit62 |
| --- | --- |
| Receiver RMTT00001 | 0 |
| ScePartyDaemon NPXS21002 | 1 |
| SceShellCore NPXS20000 | 1 |
| SceSysCore NPXS20991 | 1 |

Raw authorization structures were not recorded. Private evidence is
../review/live-229/authinfo-contexts.json and check_authinfo.py. These results
explain the meaning of the observed Mbus classification, but do not prove a
new service inherits that bit or that changing it alone permits microphone
routing. Do not substitute a new fake-SELF paid value for a verified runtime
capability result.

## Execution choices

| Choice | Evidence | Remaining obstacle |
| --- | --- | --- |
| Ordinary app or game plugin | Version0.03 registers with system flag0 and fails Vmic notification | Does not supply the required authorization |
| Separate BlueHEN process | Preferred ownership/lifetime model | Spawn/bootstrap, authorization and background survival unverified |
| Module hosted by an existing system process | SDK exposes process PRX load/unload; some host processes have bit62 | Shares host lifetime/resources; unloading and routing ownership require validation |
| Existing HearBridge binloader payload | Its launcher sends a payload to9090; previous local probe ran inside PartyDaemon | The word daemon does not establish an independent PID; also uses a USB transport |

The public GoldHEN SDK exposes named-process PRX loading and unloading. Its
wrapper is not a standalone process-spawn API. SDK availability also does not
prove the proprietary loader accepts every target or guarantees safe unload.
Its jailbreak/unjailbreak interfaces exist, but their capability effects have
not been measured here. No authorization-changing experiment was performed.

## Next implementation gate

Build a lifecycle-only standalone prototype before adding microphone calls:

1. Select and inspect a 9.60-compatible process bootstrap. A long-running
   thread inside PartyDaemon does not satisfy the separate-process requirement.
2. Record the new process's own PID/title, parent relationship and get_authinfo
   result, without relying on inherited or guessed privileges.
3. Verify it survives launcher exit and foreground app changes, then accepts
   one explicit stop and exits without leaving threads or IPC endpoints.
4. Only after those checks, rerun the bounded Vmic lifecycle with exact ownership
   checks. Negative Create can still allocate a slot; retain current cleanup.

Keep transport, audio routing and lifecycle separate in the implementation.
The service should own its handles, accept only bounded local control messages,
and use an explicit state sequence: starting, ready, active, stopping, stopped
or failed. Startup must publish identity before enabling audio; failure must
stop ingress and clean only resources it acquired. Game-side hooks, if needed,
are separate clients rather than owners of the microphone resource.

Built-in Bluetooth transport is still unimplemented. Headphone playback and
microphone input also require different profile capabilities; a playback-only
sink cannot be assumed to provide microphone audio. No pairing, PCM capture,
game chat or party-chat acceptance has been demonstrated.

## Primary references checked

- [GoldHEN SDK interface](https://github.com/GoldHEN/GoldHEN_Plugins_SDK/blob/bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7/include/GoldHEN.h)
- [GoldHEN SDK implementation](https://github.com/GoldHEN/GoldHEN_Plugins_SDK/blob/bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7/source/GoldHEN.c)
- [Official plugin repository](https://github.com/GoldHEN/GoldHEN_Plugins_Repository)
- [shadPS4 get_authinfo implementation and ABI](https://github.com/shadps4-emu/shadPS4/pull/3760/files)
