# Built-in Bluetooth transport: 9.60 investigation

## 00.15 installed and running; confirmation test pending

Receiver PID111/task-1 and the same reboot7 session were verified. The
hash-matched 00.14 was uninstalled (return0);00.15 installed on BLHN00007 with
full readback matching6f21821e89140008b45fcca5581f5f48c1422d6db5152ee2292084414461b82b.
Own PID114 passed auth/code gates, reported UI_READY pad_ok1 and first
presentation0. ShellUI BT globals stayed -1/-1. Launch ticket7 consumed once
(0x60001b0d). The app remains open for the user to select the headset, press
Square and accept its confirmation with X. No console reboot was performed.
Confirmation reply, completed connection and cleanup still await this test;
headset audio/microphone remain unimplemented. Private evidence:
replace-v14-for-ui-v15.json, service-ui-v15-side-install.json,
service-ui-v15-run.json/console.log, launch-BLHN00007-device-ui-v15.json.

## 00.14 reached confirmation; 00.15 adds explicit on-screen acceptance

Physical 00.14: scan result0/count2, selected slot1, GetConnectingInfo returned1,
StartConnect returned0, state2 observed repeatedly, then target event type4
status3. The diagnostic disconnected (return0), observed state1, deleted its
queue and closed both descriptors (all0). Final result63. This is a user
confirmation request, distinct from PIN entry (event3); the old combined UI
wording led the user to report a PIN request. Connection completion, pairing
registration, streaming and microphone transport remain unproved.

00.15 asks for X acceptance on that target confirmation event, or Circle to
cancel. No acceptance is automatic. A bounded wait and cancellation send a
rejection. Only one confirmation is answered per attempt. ReplyUserConfirmation
uses ioctl0x80106208 with a16-byte envelope: address u64 at0, accept u32 at8,
zero padding at12. Own native wrapper0x3a2c10 and managed methods2681/2682 agree;
fields2290/2291 establish reject0/accept1. PIN entry remains unsupported and now
has distinct wording. The connection test still disconnects after completion;
this build does not add persistent audio or microphone operation.

Seven host tests pass (connection cases expanded to cover confirmation accept,
reject, reply failure and repeated requests); PKG validation passes. Version00.15
on stable BLHN00007,6619136 bytes, SHA256
6f21821e89140008b45fcca5581f5f48c1422d6db5152ee2292084414461b82b.
Artifact: build/service-ui-v15/IV0000-BLHN00007_00-BLUEHENSERVICE01.pkg.
Not installed. 00.14 was still active when observed; user asked to exit via
Circle/Options and reopen Receiver. No console reboot performed. Private
service-ui-v14-console.log records the completed diagnostic; ui-v14-app-slots.json
records the expected active own slot while the GUI remains open.

## GUI accepted and selected-device connection diagnostic 00.14

The user confirmed 00.13 works on the TV. Its physical log records 17 inquiry
results, three unique devices, three successful guarded name queries, scan
result0 and UI_SELECTED slot1. Names/addresses remain unlogged. Later the own
app slot and BGFT marker were clear; there is no UI_CLOSED/RESULT/exit log, so
normal in-app shutdown is not established. Preserve the exact 00.13 package.

00.14 adds Square for a bounded connection test of the selected audio-class
entry. This is a diagnostic, not persistent connection or audio support.
The test owns its fd/queue, uses a passive conflict check, and requires the
selected address to report DISCONNECTED before StartConnect. Existing
connections are not changed. Controller/non-audio classes and zero addresses
are refused. Events from other addresses cannot complete or fail the test.
PIN/user-confirmation events stop the attempt without guessing credentials or
sending acceptance. Every successfully submitted attempt requests disconnect
of only its selected address, then verifies DISCONNECTED before releasing its
queue/fd. Failure to establish cleanup closes the UI rather than retrying.
Registration records are never deleted; pairing may leave a registration.

ABI verified from own-console native wrappers: GetConnectingInfo0x3a2ad0,
StartConnect0x3a2c60, StartDisconnect0x3a2cb0 each pass pointer-to-u64 address.
Requests are0x80086203/0x8008620a/0x8008620b. Managed fields2283..2288 define
states1=disconnected,2=connecting,3=connected,4=disconnecting,5=streaming,
6=stopping. Fields2227..2230 define PIN3, confirmation4, connect-result5,
disconnect6. Major-device-class mask0x1f00 and audio/video0x400 come from
fields2315/2333. These are ABI evidence, not proof of generic audio transport.

Seven host tests pass, including connection ownership, rejection, foreign
address events, cancellation, timeout, PIN request and failed cleanup. PKG
validation passes. Same BLHN00007, version00.14,6619136 bytes, SHA256
05cd9ecb4b9ebb58cc68e4b9de548732df0d5737ed791f632a1fa346b0aa9811.
Artifact: build/service-ui-v14/IV0000-BLHN00007_00-BLUEHENSERVICE01.pkg.
Receiver PID105/task-1 verified;00.13 hash-checked/uninstalled return0;00.14
installed with matching full readback. PID108 reached READY; the first monitor
transport timed out before permission. A fresh verified observation resumed
that same process gate without relaunch. UI_READY pad_ok1 and first presentation0
then passed; ticket6 consumed once0x6000190d. The app is open awaiting the user's
Square test. Physical connection test still pending. Private journals include
service-ui-v14-run.json, service-ui-v14-resume-gate.json and the installation/
launch journals. No console reboot was performed.

## 00.13 physical startup reached first presentation

Receiver PID99/task-1 was verified. The hash-matched 00.12 was uninstalled
(return0), and 00.13 installed on the same BLHN00007 with full package readback
matching 39682d5cce7c353b293ae1a620eca501d6e0ffaebedfef5d1c1f8273acffe119.
Own PID102 loaded all three dependencies successfully, initialized SDL, created
the window/surface/software renderer, and logged UI_READY pad_ok=1 followed by
UI_FRAME_PRESENTED result=0. It remains open for manual testing. ShellUI BT
globals stayed -1/-1. Ticket5 was consumed exactly once (0x6000170d).

This establishes successful API startup/presentation on hardware; visible TV
output, controller navigation, discovery names and selection still require user
confirmation. No pairing, connection, headset audio or microphone is implemented.
Private evidence: replace-v12-for-ui-v13.json, service-ui-v13-side-install.json,
service-ui-v13-run.json/console.log, launch-BLHN00007-device-ui-v13.json.

## GUI startup investigation: 00.12 exited; 00.13 ready for testing

00.12 was installed with full package readback verification. PID96 reached the
permission gate but exited before UI_READY, without RESULT or exit logging.
No inquiry started. The console remained responsive, own app slot/BGFT marker
were clear, and ShellUI Bluetooth globals stayed -1/-1. Launch ticket4 was
consumed once (0x6000150d). Neither graphics nor headset identity is confirmed.
Private evidence: service-ui-v12-run.json/console.log/side-install.json,
ui-v12-exit-app-slots.json and launch-BLHN00007-device-ui-v12.json.

00.13 explicitly loads UserService, VideoOut and Pad before SDL, records each
initialization stage and the first window-surface presentation result, and
cleans up partially initialized graphics. Local SDL disassembly calls
sceVideoOutOpen without loading VideoOut in that function; this motivates the
change but does not establish the cause of the 00.12 exit. Six host tests and
PKG validation passed. Physical startup remains untested; Receiver needs reopening.
The run harness now requires both controller readiness and a successful first
presentation, and detects process disappearance without waiting for exit logs.
Artifact: build/service-ui-v13/IV0000-BLHN00007_00-BLUEHENSERVICE01.pkg
SHA256: 39682d5cce7c353b293ae1a620eca501d6e0ffaebedfef5d1c1f8273acffe119
Size: 6619136 bytes. Same stable BLHN00007; no new application title.

## Physical inquiry passed: 00.11, reboot7

User supplied a UGreen headset in pairing mode. Receiver PID86/task-1 was
verified;00.11 replaced the hash-matched00.10 on stable BLHN00007, with full
readback matching fd4035c0...f178cf7. Own PID89 passed the auth and code gates.
Open, eight-byte subscription and kqueue registration succeeded. Inquiry start
returned0. Thirteen result events represented two distinct addresses, with
observed value fields0x0008043c and0x00240404. No names/addresses were logged.
This is successful discovery, NOT confirmed identification of the UGreen device.
The per-result status bytes (e.g.199..209) are recorded without interpreting them
as errors; their semantics have not yet been established.

Stop returned0, filter delete0, queue close0, fd close0. RESULT0 and LoadExec exit
were observed; own ProcessStarter slot and BGFT marker were released. ShellUI
manager globals remained-1/-1. Ticket3 consumed once (0x6000130d). The user
confirmed controller/menu operation afterwards and reported no headset connection,
which is expected: no pairing/connect call was sent.
Private evidence: service-scan-v11-run.json/console.log/side-install.json,
scan-v11-app-slots.json, replace-v10-for-scan-v11.json,
launch-BLHN00007-normal-scan-v11.json. Preserve this exact tested package.

## Graphical device selector 00.12 prepared, not installed

The user requested a graphical interface for equipment selection. The new
main_device_ui.c uses native SDL software rendering plus Pad input on PS4, the
same stable gd title/auth profile and the proven own-fd inquiry flow. Triangle
starts discovery; up/down changes focus; X selects an in-memory device; Circle
cancels discovery or exits; Options exits. The list is populated by actual inquiry
events. New scans clear the previous list and selection. No device is auto-connected.
The UI explicitly says connection, audio and microphone are not ready.

After a successful stop, it attempts a bounded name lookup for each discovered
address using ioctl0xc0106206, envelope{u64 address,pointer name}. The native
wrapper at0x3a2b60 and managed method2677 establish the envelope; metadata field2217
sets maximum name length120. A guarded128-byte buffer checks the trailing guard
and bytes121..127, requires termination within121 bytes, and sanitizes display
text. Names appear on screen only; logs contain result/guard/slot, not names or
addresses. Name lookup is untested on hardware and no hard ioctl timeout is claimed.

Local preview renders the actual UI drawing code with explicitly labelled example
data; this is layout evidence, not PS4 graphics proof. Six host tests pass,
including GUI inquiry cleanup and cancellation before/after start. Warnings-as-
errors compilation, fake-SELF validation and package validation passed.
Artifact build/service-ui-v12/IV0000-BLHN00007_00-BLUEHENSERVICE01.pkg,
6619136 bytes, SHA256
22b8f8cc3b5a7d6e1fb2dee5fc7fd675e7e35de237af034d93874207dc2cea0f.
NOT installed or physically tested.00.11 remains installed. User asked to reopen
Receiver and report ready. UI exits after five minutes without button input and
supports a PID-bound service-ui-v12.stop file for controlled test shutdown.

Next: verify Receiver and same-boot sessions, replace00.11, launch and fingerprint
exports before granting the gate. run_device_ui_v12.py stops observing at UI_READY
with pad_ok1 and intentionally leaves the UI running for the user. Obtain physical
TV/input/name-selection evidence; then observe exit and clean slot/BGFT state and
consume the new launch ticket once. No pairing/audio acceptance is implied.

## Prepared bounded inquiry 00.11 (not physically tested)

The start/stop wrappers take no input payload: ioctl0x20046204/0x20046205 with
third argument0. ShellUI opens its own fd, writes an eight-byte0xfe subscription,
creates a kqueue and registers EVFILT_READ(-1), EV_ADD(1). Poll uses zero timeout
and reads16-byte events. Close removes the filter (EV_DELETE2), closes its queue
and closes its own fd; no controller reset is present in that observed path.
Managed metadata confirms event types1=inquiry result,2=inquiry stop, and the
layout u8 type/status/reserved[2],u32 value,u64 address. The meaning of value in
an inquiry event has not yet been validated; logs call it value, not a profile.

Four native code ranges matched live ShellUI42 in reboot7; its manager fd and
queue globals were both-1. Private evidence: inquiry-code-reboot7.json. This is
not proof of multi-client event delivery or radio exclusivity. The host harness
requires those globals to remain inactive before launch and observes them after.

00.11 uses stable BLHN00007/gd and the same private profile. Own fd additionally
uses O_NONBLOCK; kevent is zero-timeout and read tolerates EAGAIN/EINTR. It observes
40x50ms passive polls, refuses inquiry traffic there, then submits one start.
After accepted start it polls at most300x50ms, at most128 events, keeps at most32
addresses only in memory, logs no names/addresses, and requests stop on every
post-start cleanup path. It never sends stop when its start failed or was not
attempted. The timing bounds apply to polling; they cannot guarantee kernel
ioctl completion time or exclusivity against an independently started scan.
It deletes its own filter and closes both own descriptors before LoadExec exit.
No pairing/connect/disconnect, credential changes or audio operations.

Host fake-driver tests exercise actual inquiry code for normal completion,
start rejection, passive inquiry conflict, read failure, short subscription write
and stop failure. The five-test suite passes; this is not hardware proof.
Compiler warnings-as-errors, fake-SELF and PKG validation passed.
Artifact build/service-scan-v11/IV0000-BLHN00007_00-BLUEHENSERVICE01.pkg,
6619136 bytes, SHA256
fd4035c08f5d93751b796cc559038280b7114a520963a0d7935ce0c38f178cf7.
Not installed/run.00.10 remains installed. User asked to reopen Receiver, put a
headset in pairing mode, keep PS4 Bluetooth settings closed, and report ready.
Next: Receiver PID/task and same-boot sessions must be verified before replacing
00.10; the own-process kqueue/kevent exports are fingerprinted before the PID gate.
Afterwards check stop/close results, process/slot/BGFT cleanup and physical
controller usability separately. Discovered events alone are not pairing/audio.

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
