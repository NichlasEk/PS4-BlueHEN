# PS4 system / home-screen audio capture (experimental)

Developer notes for tools that are not part of the HearBridge app. The app
streams game audio only; the injectors below are unfinished and unsupported.

Goal: stream **home screen / SceShellUI** sounds into the HearBridge daemon
(UDP `127.0.0.1:9500`).

## Why not plugins.ini alone?

GoldHEN's `plugins.ini` loads PRXs when a **title** starts (`[default]` or
`[CUSA...]`). SceShellUI (`NPXS20001`) is already running before you can edit
ini files, so an ini section does not inject into the live ShellUI process.

## Chosen path (one payload)

1. Build `shell_audio_tap.prx` and the minimal `shell_tap_test.prx`
   (`make plugin`). The audio PRX hooks
   `sceAudioOutOpen/Close/Output/Outputs`, converts to 48 kHz s16 stereo,
   and `sendto`s to UDP 9500 with `MSG_DONTWAIT` (drops when busy; never
   blocks the audio thread). Socket is created lazily on first tee.
2. **Safer injectors** (`make inject` / `make inject-test`):
   - Refuse unless `/data/hearbridge/state` is `streaming`
   - Locate SceShellUI via `sysctl`, refuse if `tap.loaded` matches its pid
   - Write each step to `/data/hearbridge/inject.log` with `fsync`
   - Embed + write the PRX, then GoldHEN `PROCESS_PRX_LOAD` into SceShellUI
     only (no other process), trying the sandbox paths in
     `src/inject_common.h` until one loads
3. **Test order on console:** daemon then `inject_test_payload.elf` (check
   `tap.log` says `loaded`) then reboot/clear marker then
   `inject_shell_audio_payload.elf` then move in the home menu.
4. Daemon receives PCM and encodes SBC to the headset.

## Optional: plugins.ini for games

The same PRX can also be listed under a game title ID if you want in-game
UI/overlay ports later. That is separate from home-screen capture.

## Risks

- Injecting into SceShellUI can crash the UI if hooks conflict with Game
  Overlay or another plugin. Disable Game Overlay while testing.
- Port formats are tracked via `sceAudioOutOpen`; buffers opened before the
  tap loaded use a 256-frame s16 stereo assumption.
- GoldHEN must expose syscall 500 (SDK). Without GoldHEN, the injector fails
  cleanly with a notification.
- UDP drops under load become silence on the headset (by design).
