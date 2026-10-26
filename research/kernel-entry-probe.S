/* SPDX-License-Identifier: GPL-3.0-or-later
 * One-shot syscall-11 entry observation for the verified 9.60 ABI.
 * Argument: pointer to 16 bytes of caller-owned, writable output memory.
 * Writes only CS and a completion marker to that buffer. No kernel offsets,
 * kernel data writes, allocation, Bluetooth calls, or SMP callbacks.
 * The caller must validate syscall 11 and keep code/output mapped until return.
 */
.text
.global bluehen_kernel_entry_probe
.type bluehen_kernel_entry_probe,@function
bluehen_kernel_entry_probe:
    mov %rdi, %rsi
    lea observe_entry(%rip), %rdi
    mov $11, %eax
    mov %rcx, %r10
    syscall
    ret
.size bluehen_kernel_entry_probe,.-bluehen_kernel_entry_probe

/* sysent handler jumps to args[0], retaining thread in RDI and args in RSI. */
.type observe_entry,@function
observe_entry:
    mov 8(%rsi), %rdx
    xor %eax, %eax
    mov %cs, %ax
    mov %rax, (%rdx)
    movabs $0x4248454e54525931, %rax
    mov %rax, 8(%rdx)
    xor %eax, %eax
    ret
.size observe_entry,.-observe_entry
.section .note.GNU-stack,"",@progbits
