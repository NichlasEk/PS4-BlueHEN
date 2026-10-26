/* SPDX-License-Identifier: GPL-3.0-or-later */
#define bh_control bh_core_control
#include "bt-hook-control.c"
#undef bh_control
#include "bt-auto-sha256.h"
#include "bt-auto-fingerprints.h"

static uint64_t bh_ticks(void){uint32_t a,d;__asm__ volatile("rdtsc":"=a"(a),"=d"(d));return ((uint64_t)d<<32)|a;}
static int bh_windows(uintptr_t base,struct bh_state *s)
{
 const uint64_t nops=UINT64_C(0x9090909090909090);
 return equal((void*)(base+BH_PRIMARY),s && s->installed?s->primary_patch:bh_primary_original,6)
  && equal((void*)(base+BH_SECONDARY),s && s->installed?s->secondary_patch:bh_secondary_original,8)
  && equal((void*)(base+BH_PRIMARY_SLOT),s && s->installed?(void*)&s->primary_pointer:(void*)&nops,8)
  && equal((void*)(base+BH_SECONDARY_SLOT),s && s->installed?(void*)&s->secondary_pointer:(void*)&nops,8);
}
static int bh_fingerprints(uintptr_t base)
{
 /* Normalize ONLY the four independently checked patch windows. */
 for(unsigned i=0;i<sizeof(bh_fingerprints_960)/sizeof(bh_fingerprints_960[0]);i++){
  const struct bh_fingerprint *f=&bh_fingerprints_960[i];struct bh_sha h;unsigned char out[32];bh_sha_init(&h);
  for(unsigned j=0;j<f->size;j++){
   unsigned off=f->offset+j;unsigned char b=*(volatile unsigned char *)(base+off);
   if(off>=BH_PRIMARY && off<BH_PRIMARY+6)b=bh_primary_original[off-BH_PRIMARY];
   if(off>=BH_SECONDARY && off<BH_SECONDARY+8)b=bh_secondary_original[off-BH_SECONDARY];
   if((off>=BH_PRIMARY_SLOT && off<BH_PRIMARY_SLOT+8)||(off>=BH_SECONDARY_SLOT && off<BH_SECONDARY_SLOT+8))b=0x90;
   bh_sha_byte(&h,b);
  }
  bh_sha_end(&h,out);if(!equal(out,f->sha,32))return 0;
 }
 return 1;
}
static void bh_report(struct bh_request *r,struct bh_state *s)
{
 r->installed=s->installed;r->enabled=s->enabled;r->generation=s->generation;
 r->primary_hits=s->primary_hits;r->primary_allowed=s->primary_allowed;
 r->secondary_hits=s->secondary_hits;r->secondary_allowed=s->secondary_allowed;
 r->cpu_mask=s->cpu_mask;r->serial_mask=s->serial_mask;r->result=0;r->complete=BH_MAGIC;
}
int bh_control(void *thread,const uintptr_t *args) HIDDEN;
int bh_control(void *thread,const uintptr_t *args)
{
 struct bh_request *r=(void*)args[1];uint32_t lo,hi;
 __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000082));
 uintptr_t base=(((uint64_t)hi<<32)|lo)-0x1c0;
 if(base<0xffffffff00000000ULL || (base&0x3fff) || !r->owner || r->command>BH_PREFLIGHT)return 22;
 if(r->base && r->base!=base)return 22;
 r->base=base;
 if(r->tsc_hz<100000000 || r->tsc_hz>10000000000ULL)return 22;
 /* Verified syscall 11 ABI and fixed firmware code before allocator/SMP use. */
 if(*(volatile uint32_t*)(base+0x10f92f0+11*0x30)!=2 || *(volatile uint64_t*)(base+0x10f92f0+11*0x30+8)!=base+0x15a6d)return 22;
 if(!bh_fingerprints(base))return 16;
 struct bh_state *s=NULL;
 if(r->resident){
  if(r->resident<0xffff800000000000ULL || r->resident>UINT64_MAX-0x4000 || (r->resident&0x3fff))return 22;
  if(!equal((void*)r->resident,bh_blob_start,BH_STATE_OFFSET))return 16;
  s=(void*)(r->resident+BH_STATE_OFFSET);
  if(s->magic!=BH_MAGIC || s->base!=base || s->owner!=r->owner || s->resident!=r->resident || s->tsc_hz!=r->tsc_hz || s->installed>1 || s->enabled>1)return 22;
  if(r->command!=BH_RETARGET && r->command!=BH_INSPECT && s->target!=r->target)return 22;
 }
 if(!bh_windows(base,s))return 16;
 if(r->command==BH_INSPECT){if(!s)return 22;r->target=s->target;bh_report(r,s);return 0;}
 if(r->command==BH_PULSE || r->command==BH_RETARGET){
  if(!s)return 22;
  uint64_t unlocked=0;
  if(!__atomic_compare_exchange_n(&s->manager,&unlocked,1,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return 16;
  int error=0;
  if(r->command==BH_PULSE){if(!s->installed || !s->enabled)error=22;else __atomic_store_n(&s->deadline,bh_ticks()+2*s->tsc_hz,__ATOMIC_SEQ_CST);}
  else if(s->installed || s->enabled || !r->target || r->target>>48)error=22;
  else s->target=r->target;
  if(!error)bh_report(r,s);
  __atomic_store_n(&s->manager,0,__ATOMIC_SEQ_CST);return error;
 }
 if(r->command==BH_ENABLE){if(!s)return 22;__atomic_store_n(&s->deadline,bh_ticks()+2*s->tsc_hz,__ATOMIC_SEQ_CST);}
 if(r->command==BH_DISABLE || r->command==BH_REMOVE){if(!s)return 22;__atomic_store_n(&s->deadline,0,__ATOMIC_SEQ_CST);}
 /* Require an idle, discovered audio sink before preparing/installing/enabling. */
 if(r->command==BH_PREFLIGHT || r->command==BH_PREPARE || r->command==BH_INSTALL || r->command==BH_ENABLE){
  if(!r->target || r->target>>48)return 22;
  uintptr_t ctx=*(volatile uintptr_t*)(base+0x2606098);if(ctx<0xffff800000000000ULL || ctx>=0xffffffff00000000ULL || (ctx&7))return 22;
  unsigned matches=0;
  for(unsigned i=0;i<32;i++){volatile unsigned char *rec=(void*)(ctx+0x18da8+i*0x408);
   if((*(volatile uint64_t*)rec&UINT64_C(0xffffffffffff))!=r->target)continue;
   uint32_t cod=*(volatile uint32_t*)(rec+8),services=*(volatile uint32_t*)(rec+0x20);
   if((cod&0x1f00)!=0x400 || (services&0x48)!=8)return 22;
   matches++;
  }
  if(matches!=1)return 22;
  for(unsigned i=0;i<7;i++)if((*(volatile uint64_t*)(ctx+0x42a8+i*0x1588+0x30)&UINT64_C(0xffffffffffff))==r->target)return 16;
  if(*(volatile uintptr_t*)(base+0x2606098)!=ctx)return 16;
 }
 if(r->command==BH_PREFLIGHT){r->complete=BH_MAGIC;r->result=0;return 0;}
 int rc=bh_core_control(thread,args);
 if(!rc && r->command==BH_PREPARE && r->complete==BH_MAGIC){s=(void*)(r->resident+BH_STATE_OFFSET);s->tsc_hz=r->tsc_hz;}
 return rc;
}
