/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BH_AUTOMATIC
#define BH_AUTOMATIC
#endif
#define BH_TRANSACTION_HOST_TEST
#include "bt-auto-control.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
const unsigned char bh_blob_start[1]={0},bh_blob_end[1]={0};
unsigned bh_test_cpu(void){return 0;}
uintptr_t bh_test_cr0_read(void){return 0x80050033;}
void bh_test_cr0_write(uintptr_t v){(void)v;}
void bh_test_after_copy(volatile unsigned char *p,size_t n){(void)p;(void)n;}
int main(int argc,char **argv)
{
 assert(argc==2);unsigned char *data=calloc(1,0x600000);assert(data);FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(data,1,0x600000,f)==0x600000);fclose(f);
 assert(bh_fingerprints((uintptr_t)data));assert(bh_windows((uintptr_t)data,NULL));
 struct bh_state s={.installed=1,.primary_pointer=123,.secondary_pointer=456};
 memset(s.primary_patch,0xab,6);memset(s.secondary_patch,0xcd,8);
 memcpy(data+BH_PRIMARY,s.primary_patch,6);memcpy(data+BH_SECONDARY,s.secondary_patch,8);
 memcpy(data+BH_PRIMARY_SLOT,&s.primary_pointer,8);memcpy(data+BH_SECONDARY_SLOT,&s.secondary_pointer,8);
 assert(bh_fingerprints((uintptr_t)data));assert(!bh_windows((uintptr_t)data,NULL));assert(bh_windows((uintptr_t)data,&s));
 data[BH_PRIMARY]^=1;assert(!bh_windows((uintptr_t)data,&s));data[BH_PRIMARY]^=1;
 for(unsigned i=0;i<sizeof(bh_fingerprints_960)/sizeof(bh_fingerprints_960[0]);i++){
  unsigned off=bh_fingerprints_960[i].offset;data[off]^=1;assert(!bh_fingerprints((uintptr_t)data));data[off]^=1;
 }
 /* Real boot gate, including stale-pointer inputs; no resident dereference. */
 unsigned char *memory=calloc(1,0x214c220);assert(memory);
 uint64_t *boot=(void*)(memory+0x214c210);boot[0]=1234567;boot[1]=765432;
 struct bh_request req={.command=BH_BOOT};
 assert(!bh_boot_identity((uintptr_t)memory,&req));
 assert(req.boot_sec==boot[0] && req.boot_usec==boot[1] && req.complete==BH_MAGIC);
 req.resident=1;assert(bh_boot_identity((uintptr_t)memory,&req));req.resident=0;
 req.command=BH_INSPECT;req.owner=1;req.tsc_hz=1600000000;
 req.resident=1;assert(!bh_boot_identity((uintptr_t)memory,&req));
 boot[0]++;assert(bh_boot_identity((uintptr_t)memory,&req));boot[0]--;
 boot[1]=1000000;assert(bh_boot_identity((uintptr_t)memory,&req));
 boot[1]=0;boot[0]=0;assert(bh_boot_identity((uintptr_t)memory,&req));free(memory);
 const unsigned char abc[32]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
 unsigned char hash[32];bh_hash("abc",3,hash);assert(!memcmp(hash,abc,32));
 puts("integrity_passed: SHA256, normalized installed windows, all fingerprint foreign-code conflicts");free(data);
}
