/* SPDX-License-Identifier: GPL-3.0-or-later */
.section .text.entry,"ax",@progbits
.global bluehen_smp_probe
bluehen_smp_probe:
    mov %rdi, %rsi
    lea bluehen_smp_observe(%rip), %rdi
    mov $11, %eax
    mov %rcx, %r10
    syscall
    ret

/* Copied into kernel-owned memory before calling smp_rendezvous.
 * No relocations or references to the caller's user address space.
 * RDI points to kernel-owned { uint64_t mask, count, invalid_cpu; }.
 */
.section .text.action,"ax",@progbits
.global bluehen_smp_action_start, bluehen_smp_action_end
bluehen_smp_action_start:
    mov %gs:0x34, %eax
    cmp $64, %eax
    jae invalid_cpu
    lock bts %rax, (%rdi)
    lock incq 8(%rdi)
    ret
invalid_cpu:
    lock incq 16(%rdi)
    ret
bluehen_smp_action_end:
.section .note.GNU-stack,"",@progbits
