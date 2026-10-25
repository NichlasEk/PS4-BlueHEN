/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
/* Offline host-only execution. Exit stubs replace destinations in this child's
 * private mapping; the firmware input and console are never modified. */
__asm__(".text\n.global call_gate\ncall_gate:\npush %r12\nmov %rdi,%r12\nmov $0x5b2dad,%eax\ncall *%rax\npop %r12\nret\n");
extern int call_gate(void *node);
/* Candidate is exercised ONLY in this host process, never on the console.
 * Synthetic target, not the user's Bluetooth address. Preserve scratch regs. */
__asm__(".text\n.global candidate_gate\ncandidate_gate:\n"
 "cmpl $0x40,0x20(%rax)\nje 1f\npush %r11\npush %r10\n"
 "mov (%rax),%r11\nshl $16,%r11\nshr $16,%r11\n"
 "movabs $0x112233445566,%r10\ncmp %r10,%r11\njne 2f\n"
 "mov 8(%rax),%r11d\nand $0x1f00,%r11d\ncmp $0x400,%r11d\njne 2f\n"
 "testl $0x40,0x20(%rax)\njnz 2f\ntestl $8,0x20(%rax)\njz 2f\n"
 "pop %r10\npop %r11\n1: jmp 0x5b2efa\n"
 "2: pop %r10\npop %r11\njmp 0x5b2f24\n");
extern void candidate_gate(void);
static void put32(unsigned char *p,uint32_t v){memcpy(p,&v,4);}
static void put64(unsigned char *p,uint64_t v){memcpy(p,&v,8);}
struct test {const char *name;uint32_t services,flags;uint16_t vid,pid;uint8_t source,state;int expected;};
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
 unsigned char *code=mmap((void*)0x5b2000,0x2000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(code==(void*)0x5b2000);
 assert(!fseek(f,0x5b2000,SEEK_SET));assert(fread(code,1,0x2000,f)==0x2000);fclose(f);
 const unsigned char signature[]={0x83,0x78,0x20,0x40,0x75,0x2a};assert(!memcmp(code+0xef4,signature,sizeof(signature)));
 const unsigned exits[]={0xefa,0xf24,0xe44,0xeb8};
 for(unsigned i=0;i<4;i++){code[exits[i]]=0xb8;put32(code+exits[i]+1,i);code[exits[i]+5]=0xc3;}
 assert(!mprotect(code,0x2000,PROT_READ|PROT_EXEC));
 struct test tests[]={
 {"observed_hp206",0x38,0x882000,0,0,0,0,1},
 {"generic_hid",0x40,0,0,0,0,0,0},
 {"mixed_hid_audio",0x48,0,0,0,0,0,1},
 {"sony_controller",0x40,0,0x54c,0x5c4,2,0,0},
 {"unknown_audio",8,0,0,0,0,0,1},
 {"special_flag",8,0x40000,0,0,0,0,2},
 {"existing_state",8,0,0,0,0,1,3},
 {"wrong_class",8,0,0,0,0,0,1},
 {"source_only",4,0,0,0,0,0,1},
 {"no_services",0,0,0,0,0,0,1}};
 for(int patched=0;patched<2;patched++) {
  if(patched){
   assert(!mprotect(code,0x2000,PROT_READ|PROT_WRITE));
   intptr_t delta=(intptr_t)candidate_gate-(0x5b2ef4+5);assert(delta>=INT32_MIN && delta<=INT32_MAX);
   code[0xef4]=0xe9;put32(code+0xef5,(uint32_t)(int32_t)delta);code[0xef9]=0x90;
   assert(!mprotect(code,0x2000,PROT_READ|PROT_EXEC));
  }
  for(unsigned i=0;i<sizeof(tests)/sizeof(tests[0]);i++)for(int selected=0;selected<3;selected++) {
   unsigned char node[0x640]={0},record[0x40]={0};struct test *t=&tests[i];put64(node+0x638,(uintptr_t)record);node[0x19]=t->state;
   put64(record,(selected?UINT64_C(0x112233445566):UINT64_C(0xabcdef123456)) | (selected==2?UINT64_C(0xabcd000000000000):0));put32(record+8,i==7?0x2508:0x240404);
   put32(record+0x20,t->services);put32(record+0x24,t->flags);memcpy(record+0x28,&t->vid,2);memcpy(record+0x2a,&t->pid,2);record[0x2f]=t->source;
   int result=call_gate(node);int expected=t->expected;
   if(patched && selected && (i==0 || i==4))expected=0;
   assert(result==expected);printf("candidate=%d selected=%d %s %d\n",patched,selected,t->name,result);
  }
 }
 return 0;
}
