/* SPDX-License-Identifier: GPL-3.0-or-later
 * Firmware-specific, host-validated research transaction. No automatic launch
 * or pairing. Published resident storage is deliberately retained until reboot.
 */
#include "bt-hook-layout.h"
#define HIDDEN __attribute__((visibility("hidden")))
extern const unsigned char bh_blob_start[] HIDDEN;
extern const unsigned char bh_blob_end[] HIDDEN;
extern const unsigned char bh_primary_original[] HIDDEN;
extern const unsigned char bh_secondary_original[] HIDDEN;
extern void bh_primary_trampoline(void) HIDDEN;
extern void bh_secondary_trampoline(void) HIDDEN;
struct bh_state bh_state_data __attribute__((section(".state"),visibility("hidden")))={0};

#ifdef BH_TRANSACTION_HOST_TEST
extern unsigned bh_test_cpu(void);
extern uintptr_t bh_test_cr0_read(void);
extern void bh_test_cr0_write(uintptr_t);
extern void bh_test_after_copy(volatile unsigned char *,size_t);
#else
static unsigned bh_test_cpu(void)
{
    unsigned cpu;__asm__ volatile("mov %%gs:0x34,%0":"=r"(cpu));return cpu;
}
static uintptr_t bh_test_cr0_read(void)
{
    uintptr_t value;__asm__ volatile("mov %%cr0,%0":"=r"(value));return value;
}
static void bh_test_cr0_write(uintptr_t value)
{
    __asm__ volatile("mov %0,%%cr0"::"r"(value):"memory");
}
#endif

static int equal(const volatile unsigned char *a,const unsigned char *b,unsigned n)
{
    for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;
    return 1;
}
static void copy(volatile unsigned char *a,const unsigned char *b,size_t n)
{
    for(size_t i=0;i<n;i++)a[i]=b[i];
#ifdef BH_TRANSACTION_HOST_TEST
    bh_test_after_copy(a,n);
#endif
}
static int branch(unsigned char *out,uintptr_t from,uintptr_t to,unsigned n)
{
    int64_t delta=(int64_t)to-(int64_t)(from+6);
    if(delta<INT32_MIN || delta>INT32_MAX)return 0;
    out[0]=0xff;out[1]=0x25;
    for(unsigned i=0;i<4;i++)out[2+i]=(uint32_t)delta>>(8*i);
    for(unsigned i=6;i<n;i++)out[i]=0x90;
    return 1;
}

/* All processors execute this from the kernel-owned copy, under rendezvous. */
void bh_transaction(void *unused) HIDDEN;
void bh_transaction(void *unused)
{
    (void)unused;
    struct bh_state *s=&bh_state_data;
    uint32_t cpu=bh_test_cpu();
    if(cpu>=64)return;
    __atomic_fetch_or(&s->cpu_mask,UINT64_C(1)<<cpu,__ATOMIC_SEQ_CST);
    uint64_t expected=0;
    if(!__atomic_compare_exchange_n(&s->writer,&expected,1,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return;
    volatile unsigned char *p=(void *)(s->base+BH_PRIMARY),*q=(void *)(s->base+BH_SECONDARY);
    volatile unsigned char *ps=(void *)(s->base+BH_PRIMARY_SLOT),*qs=(void *)(s->base+BH_SECONDARY_SLOT);
    const uint64_t nops=UINT64_C(0x9090909090909090);
    uint64_t oldps=s->installed?s->primary_pointer:nops,oldqs=s->installed?s->secondary_pointer:nops;
    const unsigned char *oldp=s->installed?s->primary_patch:s->primary_original;
    const unsigned char *oldq=s->installed?s->secondary_patch:s->secondary_original;
    if(!equal(p,oldp,6) || !equal(q,oldq,8) ||
       !equal(ps,(const void *)&oldps,8) || !equal(qs,(const void *)&oldqs,8)){s->result=16;return;}
    if(s->command==BH_ENABLE || s->command==BH_DISABLE){
        if(!s->installed){s->result=22;return;}
        s->enabled=s->command==BH_ENABLE;
        s->generation++;
        return;
    }
    if((s->command==BH_INSTALL && s->installed) ||
       (s->command==BH_REMOVE && !s->installed) ||
       (s->command!=BH_INSTALL && s->command!=BH_REMOVE)){s->result=22;return;}
    const unsigned char *newp=s->command==BH_INSTALL?s->primary_patch:s->primary_original;
    const unsigned char *newq=s->command==BH_INSTALL?s->secondary_patch:s->secondary_original;
    uint64_t newps=s->command==BH_INSTALL?s->primary_pointer:nops,newqs=s->command==BH_INSTALL?s->secondary_pointer:nops;
    s->enabled=0;
    uintptr_t cr0=bh_test_cr0_read();
    uintptr_t writable=cr0&~(UINT64_C(1)<<16);
    bh_test_cr0_write(writable);
    if(s->command==BH_INSTALL){copy(ps,(const void *)&newps,8);copy(qs,(const void *)&newqs,8);}
    copy(p,newp,6);copy(q,newq,8);
    if(s->command==BH_REMOVE){copy(ps,(const void *)&newps,8);copy(qs,(const void *)&newqs,8);}
    if(!equal(p,newp,6) || !equal(q,newq,8) || !equal(ps,(const void *)&newps,8) || !equal(qs,(const void *)&newqs,8)){
        copy(p,oldp,6);copy(q,oldq,8);copy(ps,(const void *)&oldps,8);copy(qs,(const void *)&oldqs,8);s->result=5;
        if(!equal(p,oldp,6) || !equal(q,oldq,8) || !equal(ps,(const void *)&oldps,8) || !equal(qs,(const void *)&oldqs,8))s->result=14;
    } else {
        s->installed=s->command==BH_INSTALL;
        s->generation++;
    }
    __asm__ volatile("mfence":::"memory");bh_test_cr0_write(cr0);
}

/* Normal action barrier precedes teardown: every CPU serializes after stores. */
void bh_serialize(void *unused) HIDDEN;
void bh_serialize(void *unused)
{
    (void)unused;
    uint32_t a=0,b,c=0,d,cpu;
    __asm__ volatile("cpuid":"+a"(a),"=b"(b),"+c"(c),"=d"(d)::"memory");
    cpu=bh_test_cpu();
    if(cpu<64)__atomic_fetch_or(&bh_state_data.serial_mask,UINT64_C(1)<<cpu,__ATOMIC_SEQ_CST);
}

int bh_control(void *thread,const uintptr_t *args) HIDDEN;
int bh_control(void *thread,const uintptr_t *args)
{
    (void)thread;
    struct bh_request *r=(void *)args[1];
    uint32_t lo,hi;
    __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000082));
    uintptr_t base=(((uint64_t)hi<<32)|lo)-0x1c0;
    if(base!=r->base || base<0xffffffff00000000ULL || (base&0x3fff) || !r->owner)return 22;
    uintptr_t state_offset=(uintptr_t)&bh_state_data-(uintptr_t)bh_blob_start;
    size_t size=(uintptr_t)bh_blob_end-(uintptr_t)bh_blob_start;
    if(state_offset!=BH_STATE_OFFSET || size>BH_STATE_OFFSET+0x1000 || r->command>BH_DISABLE)return 22;
    struct bh_state *s;
    if(r->command==BH_PREPARE){
        if(r->resident || !r->target || r->target>>48)return 22;
        if(!equal((void *)(base+BH_PRIMARY),bh_primary_original,6) ||
           !equal((void *)(base+BH_SECONDARY),bh_secondary_original,8))return 16;
        if(*(volatile uint64_t *)(base+BH_PRIMARY_SLOT)!=UINT64_C(0x9090909090909090) ||
           *(volatile uint64_t *)(base+BH_SECONDARY_SLOT)!=UINT64_C(0x9090909090909090))return 16;
        uintptr_t map=*(volatile uintptr_t *)(base+0x2147830);
        if(map<0xffff800000000000ULL || (map&7))return 22;
        uintptr_t (*allocate)(uintptr_t,size_t)=(void *)(base+0x1889d0);
        void (*release)(uintptr_t,uintptr_t,size_t)=(void *)(base+0x188ba0);
        uintptr_t resident=allocate(map,0x4000);
        if(!resident)return 12;
        copy((void *)resident,bh_blob_start,size);
        s=(void *)(resident+state_offset);
        if(!branch(s->primary_patch,base+BH_PRIMARY,base+BH_PRIMARY_SLOT,6) ||
           !branch(s->secondary_patch,base+BH_SECONDARY,base+BH_SECONDARY_SLOT,8)){
            release(map,resident,0x4000);return 34;
        }
        s->magic=BH_MAGIC;s->base=base;s->owner=r->owner;s->resident=resident;
        s->target=r->target;
        s->primary_pointer=resident+(uintptr_t)bh_primary_trampoline-(uintptr_t)bh_blob_start;
        s->secondary_pointer=resident+(uintptr_t)bh_secondary_trampoline-(uintptr_t)bh_blob_start;
        s->primary_next=base+BH_PRIMARY+6;s->primary_accept=base+0x5b2efa;
        s->secondary_next=base+BH_SECONDARY+8;s->secondary_accept=base+0x59f998;
        copy(s->primary_original,bh_primary_original,6);copy(s->secondary_original,bh_secondary_original,8);
        r->resident=resident;
    } else {
        if(r->resident<0xffff800000000000ULL || r->resident>UINT64_MAX-0x4000 || (r->resident&0x3fff))return 22;
        s=(void *)(r->resident+state_offset);
        if(s->magic!=BH_MAGIC || s->base!=base || s->owner!=r->owner || s->resident!=r->resident || s->target!=r->target)return 22;
        if(*(volatile uint32_t *)(base+0x20ff03c)!=1 || *(volatile uint64_t *)(base+0x20ff0a0)!=0xff)return 22;
        uint64_t unlocked=0;
        if(!__atomic_compare_exchange_n(&s->manager,&unlocked,1,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return 16;
        s->writer=0;s->result=0;s->cpu_mask=0;s->serial_mask=0;s->command=r->command;
        void (*pin)(void)=(void *)(base+0x457c50),(*unpin)(void)=(void *)(base+0x457c70);
        void (*rendezvous)(void (*)(void *),void (*)(void *),void (*)(void *),void *)=(void *)(base+0x52530);
        void (*action)(void *)=(void *)(r->resident+(uintptr_t)bh_transaction-(uintptr_t)bh_blob_start);
        void (*teardown)(void *)=(void *)(r->resident+(uintptr_t)bh_serialize-(uintptr_t)bh_blob_start);
        pin();rendezvous(NULL,action,teardown,NULL);unpin();
    }
    r->result=s->result;r->installed=s->installed;r->enabled=s->enabled;
    r->cpu_mask=s->cpu_mask;r->serial_mask=s->serial_mask;
    r->primary_hits=s->primary_hits;r->secondary_hits=s->secondary_hits;
    r->primary_allowed=s->primary_allowed;r->secondary_allowed=s->secondary_allowed;
    r->generation=s->generation;r->complete=BH_MAGIC;
    if(r->command!=BH_PREPARE)__atomic_store_n(&s->manager,0,__ATOMIC_SEQ_CST);
    return 0;
}
