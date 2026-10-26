# Kernel installation prerequisites on the observed PS4 9.60

## Physical entry and SMP observations, 2026-10-25

The recovery inspection found the accepted 0.17 package unchanged, the GUI
running as BLHN00007, and its last log at UI_READY / first successful frame.
No new Bluetooth attempt was present. This does not explain why the previous
host session ended.

Two one-shot probes then ran in that same GUI process through the debugger RPC:

* `kernel-entry-probe.S`: syscall 11 returned zero, CS was 0x20 (ring 0), the
  completion marker matched, and only the 16-byte output buffer changed.
* `kernel-smp-probe.c` / `.S`: allocated one temporary 16 KiB kernel page,
  copied a position-independent counting callback into it, and called the
  normal rendezvous with NULL setup/teardown callbacks, retaining its barriers.
  Eight callback visits produced mask 0xff, zero invalid CPU IDs, and completion
  stage 4. The kernel free function returned normally. Independent page-table
  verification of that free was not performed.

Both probes preserved their output guards and unmapped their host-allocated
user test pages after returning. A getpid call succeeded afterward. The
debugger RPC thread remains in this own GUI process until process exit; it is
not a persistent kernel hook. No Bluetooth function was called and no existing
kernel instruction or credential was changed. The SMP probe necessarily wrote
its temporary kernel allocation and exercised normal allocator/SMP bookkeeping.

Private evidence lives outside this source repository under
`review/recovery-20261025/`: `inspection.json`, `kernel-prerequisites.json`,
`kernel-entry-result.json`, `kernel-smp-result.json`, exact tested binaries and
their disassembly. The runner journals before every mutation, never retries a
submitted probe, and fingerprints exports, the syscall handler and kernel code.
Session PID, RPC addresses, buffers and kernel base must never be reused blindly.

## Firmware identification and reviewed offsets

The upstream [9.60 Linux-loader definitions](https://github.com/ps4-linux/ps4-linux-loader/blob/d0f4d4c0bbc9e8b4870300781b352605ed2bc91d/linux/magic.h)
and [allocator/sysent definitions](https://github.com/ps4-linux/ps4-linux-loader/blob/d0f4d4c0bbc9e8b4870300781b352605ed2bc91d/freebsd-headers/ps4-offsets/960.h)
provided candidate offsets. They were compared against the private RX capture,
disassembled, and freshly read from the running kernel before use.

| Item | Kernel-relative offset | Evidence / limitation |
| --- | --- | --- |
| Syscall 11 table entry | 0x10f92f0 + 11 * 0x30 | Two arguments; handler at 0x15a6d jumps through args[0] |
| Rendezvous action | 0x52240 | Normal setup/action/teardown barriers visible in code |
| No-barrier sentinel | 0x52340 | Must not be used for this observation |
| Rendezvous CPU subset | 0x52350 | Receives the CPU mask from the wrapper |
| Rendezvous wrapper | 0x52530 | Live code matched; counting callback ran on eight CPUs |
| All-CPU mask | 0x20ff0a0 | Observed 0xff; smp_started at 0x20ff03c was 1 |
| Pin / unpin | 0x457c50 / 0x457c70 | Surround only the rendezvous, not allocation/free |
| Kernel allocator / free | 0x1889d0 / 0x188ba0 | Temporary allocation, execution, and free return observed |
| Kernel map pointer | 0x2147830 | Read by the callback after LSTAR-derived base validation |

The observed allocator already passed protection 7 at 0x188a9c and 0x188aa4.
No allocator patch was applied. This is evidence about this running GoldHEN
session, not a promise that an unmodified 9.60 kernel permits executable pages.

## Build and review

From this repository root:

```
python3 research/build_kernel_probes.py --out build/kernel-probes-960
```

The output directory must be new. The script creates binaries, disassembly and
SHA256 metadata only; it does not install, upload or invoke either probe. Review
the disassembly as well as the relocation check: a binary without relocations
can still contain erroneous absolute references. The callback source symbols
are hidden so the compiler emits RIP-relative references before copying it.
The remote counting callback itself contains no user-space pointers.

## Remaining barrier to a Bluetooth code patch

A subsequent [single-instruction transaction prototype](BT-HOOK-TRANSACTION-960.md)
addresses the interrupted-instruction issue with different patch windows and
retains published storage until reboot. Its indirect-jump variant now passes
host tests and physical disabled installation/restoration. The constraints below explain why the earlier
multi-instruction prototype was insufficient.

Successful rendezvous is necessary but insufficient for arbitrary code
replacement. An interrupted CPU could resume inside a multi-instruction patch
window after the rendezvous. An in-flight trampoline also cannot be freed just
because the entry branch was restored; an interrupt may have arrived before its
reference count increment. Neither condition is exercised by this counting test.

Before replacing either admission branch, establish a driver quiescence or
saved-instruction-pointer protocol covering both patch windows and all callers.
Account for sleepers and code already inside the affected functions, retain the
normal controller path and capacity checks, and define ownership/recovery when
the GUI disappears. Verify original bytes again inside the synchronized
transaction, journal both sites, and handle partial installation/restoration.
Allocate kernel-owned trampoline code independently of GUI lifetime. Do not
free it until its entire entry/exit window is excluded, including the interval
before any counter increment. No such installer has been run or completed.

After that, connection admission, AVDTP/actual audio, microphone transport and
party routing remain separate physical acceptance steps.
