/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
/* Offline dispatch only. Remote-address bit skips the timer helper; teardown
 * and transport callees never run. Local stubs return 0x100 | service bit. */
static void put32(void *p,uint32_t v){memcpy(p,&v,4);}
static unsigned char *page(uintptr_t addr){
 void *p=mmap((void*)addr,4096,PROT_READ|PROT_WRITE,
 MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(p==(void*)addr);return p;
}
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
 unsigned char *code=page(0x596000);assert(!fseek(f,0x596000,SEEK_SET));
 assert(fread(code,1,4096,f)==4096);fclose(f);
 const uintptr_t callees[]={0x5aa300,0x5b7d70,0x59d490};
 for(unsigned i=0;i<3;i++){
  unsigned char *p=page(callees[i]&~(uintptr_t)4095),*q=p+(callees[i]&4095);
  /* mov edx,eax; or 0x100,eax; ret */
  const unsigned char stub[]={0x89,0xd0,0x0d,0,1,0,0,0xc3};memcpy(q,stub,sizeof(stub));
  assert(!mprotect(p,4096,PROT_READ|PROT_EXEC));
 }
 assert(!mprotect(code,4096,PROT_READ|PROT_EXEC));
 struct {const char *name;uint32_t services,active,excluded,expected;} cases[]={
  {"hp206_sink_first",0x38,0,0,0x108},
  {"hp206_sink_active",0x38,8,0,0x110},
  {"hp206_sink_excluded",0x38,0,8,0x110},
  {"hp206_profiles_active",0x38,0x18,0,0},
  {"source_before_sink",0x0c,0,0,0x104},
  {"source_active_sink_next",0x0c,4,0,0x108},
  {"sink_only",8,0,0,0x108},
  {"controller_only",0x40,0,0,0x140},
  {"no_services",0,0,0,0},
  {"low_bit_1_no_dispatch",1,0,0,0},
  {"low_bit_2_no_dispatch",2,0,0,0},
  {"all_excluded",0x7f,0,0x7f,0}};
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  unsigned char node[0x1588]={0},record[0x40]={0};uintptr_t ptr=(uintptr_t)record;
  memcpy(node+0x638,&ptr,8);node[0x36]=1;put32(node+0x18,1);
  put32(node+0x12d0,cases[i].active);put32(node+0x12e8,cases[i].excluded);
  put32(record+0x20,cases[i].services);
  uint32_t got=((uint32_t(*)(void*))0x5963d0)(node);
  if(got!=cases[i].expected){fprintf(stderr,"%s got=%x expected=%x\n",cases[i].name,got,cases[i].expected);return 1;}
  printf("%s result=0x%x\n",cases[i].name,got);
 }
 return 0;
}
