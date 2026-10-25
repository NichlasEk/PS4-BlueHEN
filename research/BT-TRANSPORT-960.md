# Built-in Bluetooth transport: 9.60 investigation

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
