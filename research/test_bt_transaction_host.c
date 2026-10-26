/* SPDX-License-Identifier: GPL-3.0-or-later
 * Executes the real transaction function with CPU/CR0 access substituted.
 * Real eight-thread writer election; no kernel code or console access.
 */
#define _GNU_SOURCE
#include "bt-hook-layout.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
const unsigned char bh_blob_start[1]={0},bh_blob_end[1]={0};
extern struct bh_state bh_state_data;
extern void bh_transaction(void *),bh_serialize(void *);
static _Thread_local unsigned cpu_id;
static _Thread_local uintptr_t fake_cr0;
static unsigned cr0_writes,faults,checks;
static unsigned fault_site=BH_SECONDARY;
unsigned bh_test_cpu(void){return cpu_id;}
uintptr_t bh_test_cr0_read(void){return fake_cr0;}
void bh_test_cr0_write(uintptr_t v){fake_cr0=v;__atomic_fetch_add(&cr0_writes,1,__ATOMIC_SEQ_CST);}
void bh_test_after_copy(volatile unsigned char *p,size_t n)
{
    if((uintptr_t)p==bh_state_data.base+fault_site && faults){p[n-1]^=1;faults--;}
}
static void *worker(void *arg)
{
    unsigned v=(uintptr_t)arg;cpu_id=v&7;fake_cr0=0x80050033;
    if(v&8)bh_serialize(NULL);else bh_transaction(NULL);
    assert(fake_cr0==0x80050033);return NULL;
}
static void operation(unsigned command,unsigned result,unsigned writes)
{
    bh_state_data.command=command;bh_state_data.writer=0;bh_state_data.result=0;
    bh_state_data.cpu_mask=bh_state_data.serial_mask=0;cr0_writes=0;
    for(unsigned stage=0;stage<2;stage++){
        pthread_t threads[8];
        for(unsigned i=0;i<8;i++)assert(!pthread_create(&threads[i],NULL,worker,(void *)(uintptr_t)(i+stage*8)));
        for(unsigned i=0;i<8;i++)assert(!pthread_join(threads[i],NULL));
    }
    assert(bh_state_data.result==result && cr0_writes==writes);
    assert(bh_state_data.cpu_mask==0xff && bh_state_data.serial_mask==0xff);
    checks++;
}
int main(void)
{
    size_t size=BH_PRIMARY+0x1000;
    unsigned char *memory=malloc(size),*expected=malloc(size);assert(memory && expected);
    memset(memory,0xa5,size);bh_state_data.base=(uintptr_t)memory;
    const unsigned char original_p[]={0x41,0xf6,0x44,0x24,0x19,1};
    const unsigned char original_q[]={0x41,0x89,0x84,0x24,0x28,0x11,0,0};
    const unsigned char patch_p[]={0xff,0x25,1,2,3,4};
    const unsigned char patch_q[]={0xff,0x25,5,6,7,8,0x90,0x90};
    bh_state_data.primary_pointer=UINT64_C(0xffffa00000001000);
    bh_state_data.secondary_pointer=UINT64_C(0xffffa00000002000);
    memcpy(bh_state_data.primary_original,original_p,6);memcpy(bh_state_data.secondary_original,original_q,8);
    memcpy(bh_state_data.primary_patch,patch_p,6);memcpy(bh_state_data.secondary_patch,patch_q,8);
    memcpy(memory+BH_PRIMARY,original_p,6);memcpy(memory+BH_SECONDARY,original_q,8);
    memset(memory+BH_PRIMARY_SLOT,0x90,8);memset(memory+BH_SECONDARY_SLOT,0x90,8);memcpy(expected,memory,size);
    operation(99,22,0);assert(!memcmp(memory,expected,size));
    operation(BH_ENABLE,22,0);assert(!memcmp(memory,expected,size));
    operation(BH_REMOVE,22,0);assert(!memcmp(memory,expected,size));
    /* Reject either conflicting site before writing the other site. */
    const unsigned sites[]={BH_PRIMARY,BH_SECONDARY,BH_PRIMARY_SLOT,BH_SECONDARY_SLOT};
    for(unsigned i=0;i<4;i++){
        unsigned off=sites[i];memory[off]^=1;expected[off]^=1;
        operation(BH_INSTALL,16,0);assert(!memcmp(memory,expected,size));memory[off]^=1;expected[off]^=1;
    }
    for(unsigned i=0;i<4;i++){
        fault_site=sites[i];faults=1;operation(BH_INSTALL,5,2);
        assert(!memcmp(memory,expected,size) && !bh_state_data.installed && !bh_state_data.generation);
    }
    fault_site=BH_SECONDARY;
    operation(BH_INSTALL,0,2);memcpy(expected+BH_PRIMARY,patch_p,6);memcpy(expected+BH_SECONDARY,patch_q,8);
    memcpy(expected+BH_PRIMARY_SLOT,&bh_state_data.primary_pointer,8);memcpy(expected+BH_SECONDARY_SLOT,&bh_state_data.secondary_pointer,8);
    assert(!memcmp(memory,expected,size) && bh_state_data.installed && !bh_state_data.enabled);
    operation(BH_INSTALL,22,0);assert(!memcmp(memory,expected,size));
    operation(BH_ENABLE,0,0);assert(bh_state_data.enabled && !memcmp(memory,expected,size));
    operation(BH_DISABLE,0,0);assert(!bh_state_data.enabled && !memcmp(memory,expected,size));
    memory[BH_SECONDARY]^=1;expected[BH_SECONDARY]^=1;
    operation(BH_REMOVE,16,0);assert(!memcmp(memory,expected,size) && bh_state_data.installed);
    memory[BH_SECONDARY]^=1;expected[BH_SECONDARY]^=1;
    operation(BH_REMOVE,0,2);memcpy(expected+BH_PRIMARY,original_p,6);memcpy(expected+BH_SECONDARY,original_q,8);
    memset(expected+BH_PRIMARY_SLOT,0x90,8);memset(expected+BH_SECONDARY_SLOT,0x90,8);
    assert(!memcmp(memory,expected,size) && !bh_state_data.installed && !bh_state_data.enabled);
    operation(BH_REMOVE,22,0);assert(!memcmp(memory,expected,size));
    faults=2;operation(BH_INSTALL,14,2);
    assert(!bh_state_data.installed && !bh_state_data.enabled && faults==0);
    assert(memcmp(memory,expected,size)!=0); /* Must report uncertain rollback. */
    printf("transaction_scenarios=%u cpu_threads_per_stage=8 guarded_full_buffer_checks=passed\n",checks);
    free(expected);free(memory);
}
