/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 9.60 ShellUI native envelope + managed interop metadata, not a portable ABI. */
#include <stdint.h>
#include <stddef.h>
#define BT_CANARY UINT64_C(0xd97a04cf61b235e8)
struct bt_registered_record {
    uint64_t address;
    uint32_t cod,services,flags;
    uint16_t vendor,product,version;
    uint8_t country_code,reserved0[5],name[121],reserved1[103];
};
struct bt_registered_query {
    uint64_t address;
    struct bt_registered_record *records;
    uint32_t capacity,padding;
};
struct bt_guarded_records {
    uint64_t before;
    struct bt_registered_record items[32];
    uint64_t after;
};
_Static_assert(sizeof(struct bt_registered_record)==256,"registered record ABI");
_Static_assert(offsetof(struct bt_registered_record,name)==32,"name offset ABI");
_Static_assert(offsetof(struct bt_registered_record,reserved1)==153,"tail offset ABI");
_Static_assert(sizeof(struct bt_registered_query)==24,"query envelope ABI");
_Static_assert(offsetof(struct bt_registered_query,capacity)==16,"capacity ABI");
_Static_assert(offsetof(struct bt_guarded_records,after)==8200,"trailing guard");
