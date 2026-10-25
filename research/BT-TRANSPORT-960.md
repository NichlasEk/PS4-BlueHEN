# Built-in Bluetooth transport: 9.60 investigation

## Physical registered-info query passed: 00.10, reboot7

Stable title BLHN00007, category gd, package SHA256
678d7138a6dc355b5c1776c56945dc7a104d87c8021f073413fab37938b7f0ab
(6619136 bytes) was installed with full readback after normal API removal of00.09.
This replacement required no console reboot. Receiver was reopened by the user.

Own process PID82 matched its intended authorization profile; no daemon
registration was used. After the PID-bound gate, open(/dev/bt,O_RDWR) succeeded.
GetRegisteredInfo ioctl0x80186201 with address0 and capacity32 returned7,
errno0. Both buffer guards were intact and close returned0. Seven nonzero
records were present, each cod0x2508/services0x40/flags0: HID device flag set,
A2DP source/sink flags clear. No device names or addresses were logged.
This physically validates enumeration with address0 for this registered set;
it does not establish inquiry, pairing, generic audio or microphone support.

RESULT0 was logged before LoadExec exit. Process exit, empty own BGFT list and
released own ProcessStarter slot were independently verified. Launch ticket2
was consumed once (0x6000110d). No scan/subscription/pair/connect/disconnect,
audio calls or credential changes occurred. Only BLHN00007 remains installed.

Private evidence: replace-v09-for-bt-v10.json, service-bt-v10-side-install.json,
service-bt-v10-run.json, service-bt-v10-console.log, bt-v10-app-slots.json,
launch-BLHN00007-normal-bt-v10.json. Exact accepted package remains on the host.
Next: recover bounded inquiry/event subscription ownership and stop behavior,
then test detection of a user-supplied headset in pairing mode. A headset live
scan is not prepared yet; registered-info success is not headset detection.
Output streaming and microphone transport remain separate unimplemented gates.

## Verified 2026-10-25

The console exposes /dev/bt, /dev/bluetooth_hid and /dev/wlanbt. A name-filtered
listing of /system/common/lib yielded libSceBluetoothHid.sprx; no similarly named
module was found in /system/priv/lib. This does not exclude another interface
under a different name or implementation inside the kernel.

Own-console orbis_audiod SHA256:
ff829eaeed8f7efc75b97e83d75f721d33c1195e7672906f2197623287db07b2.
Its /dev/bt open paths at virtual offsets0x1b6c0 and0x1e8c0 cache separate
file descriptors at0x97308 and0x9744c. The second path leads into the existing
JEDI input setup. File descriptors are process-local, not reusable handles.

Six immediate ioctl request sites were found in the local disassembly:

| Request | Sites | Encoded envelope size |
| --- | --- | --- |
| 0x80106212 | 0x10c9f | 16 |
| 0x80206216 | 0xa910, 0xa9ed, 0x17eb7 | 32 |
| 0x80206217 | 0x1ec1b, 0x1f509 | 32 |

All six 32-byte code prefixes and both open-function prefixes matched the live
audio daemon in reboot4. The scan finds immediate constants loaded into esi;
it is not an exhaustive inventory of dynamically computed requests.
No ioctl, scan, pairing, reset, disconnect, descriptor use or PCM read was sent.
Private journals: bt-paths-reboot4.json, bt-ioctl-sites.json,
bt-ioctl-live-reboot4.json under review/live-229.

## Limits and next work

The envelope size does not describe the pointed-to payload. For example the
0x17eb7 send path builds fields from an existing audio context, a buffer pointer,
a0x300 count and a zero field; their complete semantics and device identity are
not yet established. The AVDTP error strings and JEDI audio paths are evidence
of internal Bluetooth-related code, not a usable generic-headset interface.

Trace the identity and profile-selection sources feeding these contexts, then
recover a bounded discovery/query ABI before attempting radio operations.
A generic microphone needs a bidirectional headset profile; an output-only
sink cannot supply microphone audio. Preserve controller connectivity when
selecting the eventual transport design. Virtual-microphone API success is
independent of the unresolved radio transport and chat-consumer routing.

## ShellUI device manager recovered, 2026-10-25

Own-console ShellUI ELF SHA256
`6c3087d01b123837c18868a058ff9203818c445c5d2e2010580578f483bca3bd`.
Its native registration table at virtual0xb12ee0 identifies libBtDeviceManager;
name/function pairs0xb12f20..0xb12fc0 expose these wrappers:

| Name suffix (sceBt) | Function offset | ioctl request |
| --- | --- | --- |
| GetRegisteredInfo | 0x3a2a20 | 0x80186201 |
| DeleteRegisteredInfo | 0x3a2a80 | 0x80086202 |
| GetConnectingInfo | 0x3a2ad0 | 0x80086203 |
| StartInquiry | 0x3a2b20 | 0x20046204 |
| StopInquiry | 0x3a2b40 | 0x20046205 |
| GetDeviceName | 0x3a2b60 | 0xc0106206 |
| ReplyPinCode | 0x3a2bb0 | 0x80186207 |
| ReplyUserConfirmation | 0x3a2c10 | 0x80106208 |
| StartConnect | 0x3a2c60 | 0x8008620a |
| StartDisconnect | 0x3a2cb0 | 0x8008620b |
| GetDeviceId | 0x3a2d00 | 0x80106235 |

All eleven relocated name/function pairs and32-byte function prefixes match
live ShellUI PID43 (NPXS20001) in reboot4. No wrapper was called. ShellUI's
executable mapping is prot7, not prot5; verification requires read+execute bits.

The embedded managed PE in platform.dll.sprx supplies interop declarations,
field layout, fixed-array marshal lengths and constants. Full monodis failed
on an unrelated missing assembly; separate typedef/method/field/marshal/constant
table extraction succeeded. It was not executed as an assembly.
GetRegisteredInfo takes u64 address, output-record pointer, u32 capacity.
The native wrapper puts them in a24-byte envelope at offsets0,8,16. Maximum
registered entries=32. Record fields imply a256-byte native sequential layout:
u64 address; u32 class/services/flags; u16 vendor/product/version; u8 country;
reserved[5]; name[121]; reserved[103]. Name begins at32, tail at153.
C static assertions pin these sizes/offsets in service-probe/bt_registered.h.

Service bits include A2DP source4, sink8, AVRCP controller0x10/target0x20,
HID device0x40/host0x80, SCMS source0x100000/sink0x200000. This enumeration
alone does not prove that generic streaming works or that other profile bits
are absent from the kernel.

The open-manager routine0x3a2500 opens /dev/bt read/write, writes the8-byte
value0xfe, then sets up a kqueue-like notification path. WaitBtDriverEvent at
0x3a27e0 polls with a zero timeout and reads16 bytes on readiness. Managed
event fields are type/status/reserved[2]/u32 value/u64 address (16 bytes).
These startup/event operations have NOT been replayed: subscription ownership
and coexistence with ShellUI require further analysis. The prototype below
only opens its own descriptor and issues the registered-info query.

## Historical 0.07 candidate (superseded; do not install)

BLHN00008 uses the established own auth-profile/daemon registration gate, opens
its own /dev/bt descriptor, then makes one GetRegisteredInfo query with address0,
capacity32 and a zero-initialized guarded8192-byte record area. The meaning of
address0 as an enumeration request is a candidate to validate; the query may
fail or return no records. It closes the descriptor before interpreting output.
It logs class/service/flag values only, never names, addresses or pairing data.
No scan, subscription write, pairing, connect, disconnect or record deletion.
The profile/service query must succeed and guards remain intact for END0.

Built, warnings-as-errors and PKG/signature validation passed; not installed or
run yet. Four existing auth-profile tests pass; they do not validate this ioctl.
Package6619136 bytes, SHA256
`2460419d1b8a8778b21ae9d2ef925122cd982c1df8c9b50a43c2fd12c7f9652f`.
The old Receiver/session references below are historical and must not be reused.
Local scripts: install_service_app_v7.py and run_service_app_v7.py SESSION_JSON.

Private evidence: SceShellUI.elf/text/asm, platform.dll.sprx and its extracted
managed PE/table listings, bt-manager-live-reboot4.json. All stay outside Git.
