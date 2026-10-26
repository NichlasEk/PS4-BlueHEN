/* SPDX-License-Identifier: GPL-3.0-or-later */
.section .text.entry,"ax",@progbits
.global bh_blob_start
.hidden bh_blob_start, bh_control
bh_blob_start:
    mov %rdi, %rsi
    lea bh_control(%rip), %rdi
    mov $11, %eax
    mov %rcx, %r10
    syscall
    ret
.section .note.GNU-stack,"",@progbits
