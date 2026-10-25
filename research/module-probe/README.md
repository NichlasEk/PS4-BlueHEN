# Audio module loading experiment

This is a diagnostic, not working Bluetooth or microphone support.

## Version 0.03: ordinary main-thread lifecycle

The package builder now compiles `main_vmic.c` as APP_VER 00.03, retaining
BLHN00001. It borrows preloaded AudioIn and performs one Create and one
verified-handle Destroy on its ordinary main thread. No RPC thread is needed
inside the test process. No PCM, routing, Bluetooth or module unloading occurs.

The supervisor must verify firmware, title/PID, the exact installed package,
full live Create/Destroy code, and a fresh Vmic baseline before supplying
`/data/bluehen/vmic-main-v3.allow` containing the PID. The READY line in
`vmic-main-v3.log` includes both resolved function addresses for verification.

After CREATE_END, observe the new slot and recheck its owner. Supply
`vmic-main-v3.cleanup` containing decimal `PID HANDLE` only for that process's
new slot. For a failed Create with no owned slot, `PID 0` reports that result
without invoking Destroy. Cleanup waits up to 45 seconds. On timeout, only
a valid handle actually returned by Create may be destroyed automatically;
a hidden handle is never guessed. A timeout without a verified handle is
explicitly logged as unresolved and requires external post-exit observation.
The main thread leaves three seconds after Destroy for live observation.
The supervisor must archive logs and remove both control files after the run.

The package was built and validated, then FTP-transferred and read back with
SHA-256 `218f922b5f10e94ffedfbe2847d6d45a7d64d202010f5ab18028d269af622acf`.
Artifact: `build/module-app-v3/IV0000-BLHN00001_00-BLUEHENMODULE001.pkg`.
Installation/runtime verification is still pending: Receiver did not respond
and the user was asked to reopen it. Do not describe this as a physical pass.
Prior version artifacts remain separate and unchanged.

## Version 0.02

The new app requests AudioIn (internal module ID 0x80000002) through
`sceSysmoduleLoadModuleInternal`. The system module manager resolves its
dependencies. After a successful load, the probe finds AudioIn in a bounded
module inventory and resolves the three Vmic exports without calling them.
It releases its one successful acquisition through
`sceSysmoduleUnloadModuleInternal` and checks that AudioIn is absent afterward.
It refuses to proceed if AudioIn was already present or inventory is uncertain.

Version 0.02 retains title BLHN00001 and uses separate `module-app-v2.log`
and `module-app-v2.allow` files under `/data/bluehen`. The build directory is
`build/module-app-v2`; version 0.01 artifacts remain preserved separately.
The package was built, validated, transferred to the console and read back
with matching SHA-256:

`6d93b310fa88900838c5c8370e4e538a94c1f9abaad1583cb53dde93f890a8ce`

Version 0.02 was subsequently installed and its installed-package readback
matched the same hash. See the physical result below.
The host tests now cover existing-module refusal, inventory errors, load
failure, export-lookup failure with cleanup, and unload failure.

### Version 0.02 physical result, 2026-10-25

Receiver restarted as PID 110. The previous test title was absent at the
installation preflight, so a fresh installation was performed; no uninstall
or modification of another title was needed.

The isolated test ran as PID 113 with title BLHN00001. Its module inventory
already contained Ipmi, Mbus, AudioOut and AudioIn. The existing-module guard
therefore returned result 8 before attempting a sysmodule load or unload.
This is NOT a successful test of the load/unload branch: these libraries were
preloaded in this application environment.

A second, deliberately instrumented run (PID 116, same package) resolved
sceAudioInVmicCreate, sceAudioInVmicWrite and sceAudioInVmicDestroy through
sceKernelDlsym in that isolated process. All three lookups returned zero,
and the resolved addresses and 32-byte prefixes matched the local 9.60
AudioIn module. The live module handle was discovered afresh (46).
No Vmic function was called. This verifies export access in our own app,
not virtual-mic creation, audio routing or headset support.

This diagnostic installed a debugger RPC thread and scratch allocation in
the short-lived test process. It invoked getpid, module inventory and dlsym,
not AudioIn entrypoints. The test's original guard still ended with result 8
after the separate export verification. Both test processes exited and both
gate files were removed. All four Vmic slots were inactive before and after.

Raw local evidence: module-app-v2-fresh-install.json, module-app-v2-run.json,
module-app-v2-lookup-run.json and module-app-v2-lookup-console.log under
../review/live-229 (outside this repository). Launch tickets 2 and 3 have
both had their final results retrieved; do not query them again.

Next: use a borrowed preloaded AudioIn module without unloading it. Only use
the system-module acquisition/release branch when it is initially absent.
Then implement a bounded, separately validated Vmic creation/cleanup test.
The installed 0.02 artifact retains the conservative refusal; do not describe
it as already implementing that next behavior.

The native binloader experiment was refused before module loading: its PID
belonged to PartyDaemon. Do not use the binloader to run this experiment in
a system process. The separate OpenOrbis app uses title ID **BLHN00001**.

## Version 0.01 physical result, 2026-10-25

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

Version 0.02 writes `/data/bluehen/module-app-v2.log` and waits up to 60 seconds
for `/data/bluehen/module-app-v2.allow` containing its PID. Before allowing:
verify fresh firmware/process identity, title BLHN00001, no pre-existing gate
or log, and a fresh Vmic status baseline. Archive prior logs deliberately;
do not interpret an old appended END marker as the current run's result.
Remove the gate afterward and verify process exit and unchanged Vmic slots.

Module loading executes module entrypoints. Version 0.02 uses the matching
system-module APIs for its acquisition and release. It resolves the Vmic
export names but never calls their function pointers. Host tests cannot
establish the real module ABI.

The asynchronous launch-result call can return 0x80020023 (EAGAIN) before a
final result. Poll that same pending ticket; never resubmit the launch just
because it is pending. Once a final result is obtained, do not consume it
again. An app ID alone does not prove test completion.
