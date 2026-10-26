/* SPDX-License-Identifier: GPL-3.0-or-later
 * Bounded 9.60 SMP plumbing observation, NOT a Bluetooth patch installer.
 * Host must fingerprint all called functions and verify the syscall-11 ABI.
 * Only the temporary allocation and caller-owned report are modified.
 */
#include <stdint.h>
#include <stddef.h>

struct smp_report {
    uint64_t expected_base;
    uint64_t entered, stage, mask, count, invalid_cpu, freed, complete;
};
extern const unsigned char bluehen_smp_action_start[]
    __attribute__((visibility("hidden")));
extern const unsigned char bluehen_smp_action_end[]
    __attribute__((visibility("hidden")));

int bluehen_smp_observe(void *thread, const uintptr_t *args)
{
    (void)thread;
    struct smp_report *out=(void *)args[1];
    uint32_t low,high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0xc0000082));
    uintptr_t base=(((uint64_t)high<<32)|low)-0x1c0;
    out->entered=0x4248534d50524f31ULL;
    if(base!=out->expected_base || base<0xffffffff00000000ULL || (base&0x3fff))
        return 22;
    uintptr_t map=*(volatile uintptr_t *)(base+0x2147830);
    if(map<0xffff800000000000ULL || (map&7))return 22;
    uintptr_t (*allocate)(uintptr_t,size_t)=(void *)(base+0x1889d0);
    void (*release)(uintptr_t,uintptr_t,size_t)=(void *)(base+0x188ba0);
    void (*pin)(void)=(void *)(base+0x457c50);
    void (*unpin)(void)=(void *)(base+0x457c70);
    void (*rendezvous)(void (*)(void *),void (*)(void *),void (*)(void *),void *)=
        (void *)(base+0x52530);
    size_t length=(uintptr_t)bluehen_smp_action_end-(uintptr_t)bluehen_smp_action_start;
    if(!length || length>0x100)return 22;
    out->stage=1;
    uintptr_t allocation=allocate(map,0x4000);
    if(!allocation)return 12;
    volatile unsigned char *code=(void *)allocation;
    volatile uint64_t *stats=(void *)(allocation+0x100);
    for(size_t i=0;i<length;i++)code[i]=bluehen_smp_action_start[i];
    stats[0]=stats[1]=stats[2]=0;
    out->stage=2;
    pin();
    /* NULL retains both normal rendezvous barriers. Never use no-barrier. */
    rendezvous(NULL,(void (*)(void *))allocation,NULL,(void *)stats);
    unpin();
    out->stage=3;
    out->mask=stats[0];out->count=stats[1];out->invalid_cpu=stats[2];
    release(map,allocation,0x4000);
    out->freed=1; /* release returned; mapping removal is not independently read. */
    out->stage=4;
    out->complete=0x4248534d50524f31ULL;
    return 0;
}
