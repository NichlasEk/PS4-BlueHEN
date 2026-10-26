/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include "bt-hook-layout.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
struct bh_state bh_state_data;
extern void bh_primary_trampoline(void),bh_secondary_trampoline(void);
__asm__(".text\n.global invoke_primary,invoke_secondary,resume_primary,resume_secondary\n"
 "invoke_primary: push %r12; mov %rdi,%r12; mov $0x5b2dad,%eax; call *%rax; pop %r12; ret\n"
 "resume_primary: push %r12; mov %rdi,%r12; testb $1,0x19(%r12); mov $0x5b2db3,%eax; call *%rax; pop %r12; ret\n"
 "invoke_secondary: push %r12; push %rbx; mov %rdi,%r12; mov 0x638(%r12),%rbx; mov $0xa1b2c3d4,%eax; mov $0x59f966,%edx; call *%rdx; pop %rbx; pop %r12; ret\n"
 "resume_secondary: push %r12; push %rbx; mov %rdi,%r12; mov 0x638(%r12),%rbx; movl $0xa1b2c3d4,0x1128(%r12); mov $0x59f96e,%edx; call *%rdx; pop %rbx; pop %r12; ret\n");
extern int invoke_primary(void *),invoke_secondary(void *),resume_primary(void *),resume_secondary(void *);
static void u32(void *p,uint32_t v){memcpy(p,&v,4);}
static void u64(void *p,uint64_t v){memcpy(p,&v,8);}
static void branch(unsigned char *site,unsigned char *slot,void (*dest)(void),size_t n)
{
    int64_t d=(intptr_t)slot-(intptr_t)(site+6);assert(d>=INT32_MIN && d<=INT32_MAX);
    u64(slot,(uintptr_t)dest);site[0]=0xff;site[1]=0x25;u32(site+2,(uint32_t)d);memset(site+6,0x90,n-6);
}
static unsigned char *map(FILE *f,uintptr_t at,size_t n)
{
    unsigned char *p=mmap((void *)at,n,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(p==(void *)at && !fseek(f,(long)at,SEEK_SET) && fread(p,1,n,f)==n);return p;
}
struct scenario {const char *name;uint32_t services,cod,flags;uint16_t vid,pid;uint8_t source,state;};
int main(int argc,char **argv)
{
    assert(argc==2 || argc==5);FILE *f=fopen(argv[1],"rb");assert(f);
    unsigned char *p=map(f,0x5b2000,8192),*q=map(f,0x59f000,4096);fclose(f);
    const unsigned char sigp[]={0x41,0xf6,0x44,0x24,0x19,0x01};
    const unsigned char sigq[]={0x41,0x89,0x84,0x24,0x28,0x11,0,0};
    assert(!memcmp(p+0xdad,sigp,6) && !memcmp(q+0x966,sigq,8));
    const unsigned pe[]={0xefa,0xf24,0xe44,0xeb8},qe[]={0x998,0x9d6};
    for(unsigned i=0;i<4;i++){p[pe[i]]=0xb8;u32(p+pe[i]+1,i);p[pe[i]+5]=0xc3;}
    for(unsigned i=0;i<2;i++){q[qe[i]]=0xb8;u32(q+qe[i]+1,i);q[qe[i]+5]=0xc3;}
    struct bh_state *s=&bh_state_data;
    void (*primary)(void)=bh_primary_trampoline,(*secondary)(void)=bh_secondary_trampoline;
    if(argc==5){
        unsigned char *resident=mmap((void *)UINT64_C(0x10000000000),0x4000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        assert(resident==(void *)UINT64_C(0x10000000000));FILE *blob=fopen(argv[2],"rb");assert(blob);
        assert(fread(resident,1,0x4000,blob)==0x20e0);fclose(blob);
        uintptr_t a=strtoull(argv[3],NULL,0),b=strtoull(argv[4],NULL,0);assert(a<0x2000 && b<0x2000);
        primary=(void *)(resident+a);secondary=(void *)(resident+b);s=(void *)(resident+0x2000);
        assert(!mprotect(resident,0x2000,PROT_READ|PROT_EXEC));
    }
    s->primary_next=0x5b2db3;s->primary_accept=0x5b2efa;
    s->secondary_next=0x59f96e;s->secondary_accept=0x59f998;
    s->target=UINT64_C(0x112233445566);
    assert(!memcmp(q+0x658,"\x90\x90\x90\x90\x90\x90\x90\x90",8));
    assert(!memcmp(q+0x938,"\x90\x90\x90\x90\x90\x90\x90\x90",8));
    const struct scenario cases[]={
      {"hp206",0x38,0x240404,0x882000,0,0,0,0},
      {"sink",8,0x240404,0,0,0,0,0}, {"hid",0x40,0x2508,0,0,0,0,0},
      {"mixed",0x48,0x240404,0,0,0,0,0}, {"wrong_class",8,0x2508,0,0,0,0,0},
      {"source",4,0x240404,0,0,0,0,0}, {"empty",0,0x240404,0,0,0,0,0},
      {"sony",0x40,0x2508,0,0x54c,0x5c4,2,0},
      {"stock_044f",0x40,0x2508,0,0x44f,0x1000,2,0},
      {"stock_06f8a",0x40,0x2508,0,0x6f8,0xe051,2,0},
      {"stock_06f8b",0x40,0x2508,0,0x6f8,0xe052,2,0},
      {"wrong_product",0x40,0x2508,0,0x6f8,0xe053,2,0},
      {"wrong_source",0x40,0x2508,0,0x44f,0x1000,1,0},
      {"sink_source2",8,0x240404,0,0,0,2,0},
      {"special",8,0x240404,0x40000,0,0,0,0},
      {"existing",8,0x240404,0,0,0,0,1}};
    int baseline[16][2];unsigned checks=0;
    for(unsigned phase=0;phase<5;phase++){
      assert(!mprotect(p,8192,PROT_READ|PROT_WRITE) && !mprotect(q,4096,PROT_READ|PROT_WRITE));
      if(phase==1){branch(p+0xdad,q+0x658,primary,6);branch(q+0x966,q+0x938,secondary,8);}
      if(phase==4){memcpy(p+0xdad,sigp,6);memcpy(q+0x966,sigq,8);memset(q+0x658,0x90,8);memset(q+0x938,0x90,8);}
      s->enabled=phase==2;
      assert(!mprotect(p,8192,PROT_READ|PROT_EXEC) && !mprotect(q,4096,PROT_READ|PROT_EXEC));
      for(unsigned i=0;i<16;i++)for(unsigned selected=0;selected<3;selected++){
        const struct scenario *t=&cases[i];unsigned char node[0x1200]={0},rec[0x40]={0};
        uint64_t address=selected?s->target:UINT64_C(0xabcdef123456);
        if(selected==2)address|=UINT64_C(0xabcd000000000000);
        u64(rec,address);u32(rec+8,t->cod);u32(rec+0x20,t->services);u32(rec+0x24,t->flags);
        memcpy(rec+0x28,&t->vid,2);memcpy(rec+0x2a,&t->pid,2);rec[0x2f]=t->source;
        u64(node+0x638,(uintptr_t)rec);node[0x19]=t->state;
        int a=invoke_primary(node),b=invoke_secondary(node);uint32_t timer;memcpy(&timer,node+0x1128,4);
        assert(timer==0xa1b2c3d4);
        if(!phase && !selected){baseline[i][0]=a;baseline[i][1]=b;}
        int ea=baseline[i][0],eb=baseline[i][1];
        int extra=phase==2 && selected && (t->cod&0x1f00)==0x400 && (t->services&8) && !(t->services&0x40);
        if(extra){eb=0;if(!(t->state&1) && !(t->flags&0x40000))ea=0;}
        if(a!=ea || b!=eb){fprintf(stderr,"phase=%u case=%s selected=%u actual=%d/%d expected=%d/%d\n",phase,t->name,selected,a,b,ea,eb);return 1;}
        checks+=2;
        /* CPUs already past the overwritten instruction retain a valid stock continuation. */
        assert(resume_primary(node)==baseline[i][0] && resume_secondary(node)==baseline[i][1]);checks+=2;
      }
    }
    assert(!memcmp(p+0xdad,sigp,6) && !memcmp(q+0x966,sigq,8));
    assert(s->primary_allowed && s->secondary_allowed);
    assert(!memcmp(q+0x658,q+0x938,8) && q[0x658]==0x90);
    printf("checks_passed=%u phases=5 sites_restored=1 primary_allowed=%llu secondary_allowed=%llu far_blob=%d\n",checks,(unsigned long long)s->primary_allowed,(unsigned long long)s->secondary_allowed,argc==5);
}
