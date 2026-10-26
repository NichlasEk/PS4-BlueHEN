/* SPDX-License-Identifier: GPL-3.0-or-later */
#define BH_PRIMARY_NEXT 96
#define BH_PRIMARY_ACCEPT 104
#define BH_SECONDARY_NEXT 112
#define BH_SECONDARY_ACCEPT 120
#define BH_PRIMARY_HITS 128
#define BH_PRIMARY_ALLOWED 136
#define BH_SECONDARY_HITS 144
#define BH_SECONDARY_ALLOWED 152
#define BH_ENABLED 40
#define BH_TARGET 48
#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stddef.h>
#define BH_MAGIC UINT64_C(0x42484254584e3031)
#define BH_PRIMARY 0x5b2dad
#define BH_SECONDARY 0x59f966
#define BH_PRIMARY_SLOT 0x59f658
#define BH_SECONDARY_SLOT 0x59f938
enum { BH_PREPARE=0, BH_INSTALL=1, BH_REMOVE=2, BH_ENABLE=3, BH_DISABLE=4 };
struct bh_state {
    uint64_t magic, base, owner, resident;
    uint64_t installed, enabled, target, command, writer, result, cpu_mask, serial_mask;
    uint64_t primary_next, primary_accept, secondary_next, secondary_accept;
    uint64_t primary_hits, primary_allowed, secondary_hits, secondary_allowed;
    unsigned char primary_original[8],primary_patch[8],secondary_original[8],secondary_patch[8];
    uint64_t generation, manager, primary_pointer, secondary_pointer;
};
struct bh_request {
    uint64_t base, command, resident, owner, target;
    uint64_t complete, result, installed, enabled, cpu_mask, serial_mask;
    uint64_t primary_hits, secondary_hits, primary_allowed, secondary_allowed, generation;
};
_Static_assert(offsetof(struct bh_state,primary_next)==BH_PRIMARY_NEXT,"layout");
_Static_assert(offsetof(struct bh_state,secondary_allowed)==BH_SECONDARY_ALLOWED,"layout");
_Static_assert(offsetof(struct bh_state,target)==BH_TARGET,"layout");
_Static_assert(offsetof(struct bh_state,enabled)==BH_ENABLED,"layout");
_Static_assert(sizeof(struct bh_state)==224,"layout");
#endif
