/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bt-hook-layout.h"
.section .text.trampolines,"ax",@progbits
.global bh_primary_trampoline, bh_secondary_trampoline
.hidden bh_primary_trampoline, bh_secondary_trampoline, bh_state_data

/* The overwritten primary window is exactly this one six-byte instruction. */
bh_primary_trampoline:
    testb $1, 0x19(%r12)
    jnz primary_next
    pushfq
    push %rax
    push %r10
    push %r11
    lock incq bh_state_data+BH_PRIMARY_HITS(%rip)
    cmpq $1, bh_state_data+BH_ENABLED(%rip)
    jne primary_stock
#ifdef BH_AUTOMATIC
    /* Preserve RDX as well as flags/RAX: RDTSC writes EDX:EAX. */
    push %rdx
    rdtsc
    shl $32, %rdx
    or %rdx, %rax
    pop %rdx
    cmp bh_state_data+BH_DEADLINE(%rip), %rax
    jae primary_stock
#endif
    mov 0x638(%r12), %rax
    testb $4, 0x26(%rax)
    jnz primary_stock
    mov (%rax), %r11
    shl $16, %r11
    shr $16, %r11
    cmp bh_state_data+BH_TARGET(%rip), %r11
    jne primary_stock
    mov 8(%rax), %r10d
    and $0x1f00, %r10d
    cmp $0x400, %r10d
    jne primary_stock
    testl $0x40, 0x20(%rax)
    jnz primary_stock
    testl $8, 0x20(%rax)
    jz primary_stock
    lock incq bh_state_data+BH_PRIMARY_ALLOWED(%rip)
    pop %r11
    pop %r10
    pop %rax
    popfq
    jmp *bh_state_data+BH_PRIMARY_ACCEPT(%rip)
primary_stock:
    pop %r11
    pop %r10
    pop %rax
    popfq
primary_next:
    jmp *bh_state_data+BH_PRIMARY_NEXT(%rip)

/* Execute the one overwritten eight-byte store before any extra decision. */
bh_secondary_trampoline:
    mov %eax, 0x1128(%r12)
    pushfq
    push %rax
    push %r10
    push %r11
    lock incq bh_state_data+BH_SECONDARY_HITS(%rip)
    cmpq $1, bh_state_data+BH_ENABLED(%rip)
    jne secondary_stock
#ifdef BH_AUTOMATIC
    /* Preserve RDX as well as flags/RAX: RDTSC writes EDX:EAX. */
    push %rdx
    rdtsc
    shl $32, %rdx
    or %rdx, %rax
    pop %rdx
    cmp bh_state_data+BH_DEADLINE(%rip), %rax
    jae secondary_stock
#endif
    mov (%rbx), %r11
    shl $16, %r11
    shr $16, %r11
    cmp bh_state_data+BH_TARGET(%rip), %r11
    jne secondary_stock
    mov 8(%rbx), %r10d
    and $0x1f00, %r10d
    cmp $0x400, %r10d
    jne secondary_stock
    testl $0x40, 0x20(%rbx)
    jnz secondary_stock
    testl $8, 0x20(%rbx)
    jz secondary_stock
    lock incq bh_state_data+BH_SECONDARY_ALLOWED(%rip)
    pop %r11
    pop %r10
    pop %rax
    popfq
    jmp *bh_state_data+BH_SECONDARY_ACCEPT(%rip)
secondary_stock:
    pop %r11
    pop %r10
    pop %rax
    popfq
    jmp *bh_state_data+BH_SECONDARY_NEXT(%rip)

.global bh_primary_original, bh_secondary_original
.hidden bh_primary_original, bh_secondary_original
bh_primary_original:
    testb $1, 0x19(%r12)
bh_secondary_original:
    mov %eax, 0x1128(%r12)
.section .note.GNU-stack,"",@progbits
