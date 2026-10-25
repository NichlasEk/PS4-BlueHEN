# Audio module loading experiment

This is a diagnostic, not working Bluetooth or microphone support.

The native binloader experiment was refused before module loading: its PID
belonged to PartyDaemon. Do not use the binloader to run this experiment in
a system process. The separate OpenOrbis app uses title ID **BLHN00001**.

## Physical result, 2026-10-25

The app package was installed and its complete FTP readback matched:

`277e68c546ef6ac1f490648765d6bdb5523495944ae458fa37e8c341338c686b`

The first launch returned an app ID but produced no log. The diagnostic
directory was owned by root with mode 0700. After changing only
`/data/bluehen` to 0777, a second launch produced a log. This supports a
directory-access explanation; no first-run errno was captured.

The second run was verified as a new process, PID 104, title BLHN00001,
before its PID-specific gate file was supplied. Its firmware query returned
0x09600011. The first call to load `/system/common/lib/libSceIpmi.sprx`
returned **0x80020002 (ENOENT)**, and the test exited with result 3.
The file exists through root FTP, so sandbox path visibility or dependency
resolution must be investigated. This result does not establish which path
the loader failed to find. No later module or Vmic call was attempted.

The process exited and the gate file was removed. All four virtual-mic slots
were inactive with owner PID zero both before and after the test.

The next experiment should use the system module manager in the isolated app,
after checking the API and ownership/unload rules. OpenOrbis declares
`sceSysmoduleLoadModuleInternal` and `sceSysmoduleUnloadModuleInternal`, with
AudioIn ID 0x80000002 and Ipmi ID 0x8000001d. These are research leads, not
proof of success or a reason to call them in PartyDaemon.

## Build and run constraints

From the repository root:

```
make -C research/module-probe test
python3 -B research/module-probe/build_pkg.py
```

The package builder uses `OO_PS4_TOOLCHAIN` (default
`/opt/openorbis/OpenOrbis/PS4Toolchain`) and an isolated .NET extraction
directory. `CREATE_FSELF` and `LIBSSL11` can override local tool paths.
The existing installed artifact must be kept separate from later rebuilds.
SDK runtime files and packages stay local, outside version control.

The app writes `/data/bluehen/module-app-v1.log` and waits up to 60 seconds
for `/data/bluehen/module-app-v1.allow` containing its PID. Before allowing:
verify fresh firmware/process identity, title BLHN00001, no pre-existing gate
or log, and a fresh Vmic status baseline. Archive prior logs deliberately;
do not interpret an old appended END marker as the current run's result.
Remove the gate afterward and verify process exit and unchanged Vmic slots.

Load/start executes each module's entrypoint. The test acquires handles and
unloads them in reverse order, stopping on an unload failure. It resolves
the Vmic export names but never calls their function pointers.
Host tests cover firmware refusal, partial-load cleanup, reverse unloading
and stop-on-unload-failure; they cannot establish the real module ABI.

The asynchronous launch-result call can return 0x80020023 (EAGAIN) before a
final result. Poll that same pending ticket; never resubmit the launch just
because it is pending. Once a final result is obtained, do not consume it
again. An app ID alone does not prove test completion.
