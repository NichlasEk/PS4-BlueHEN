/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
static unsigned char node[0x1588],record[0x408],context[32];
static void u32(void *p,uint32_t x){memcpy(p,&x,4);}
static void u64(void *p,uint64_t x){memcpy(p,&x,8);}
static unsigned char *map(uintptr_t at,size_t size){void *p=mmap((void*)at,size,3,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(p==(void*)at);return p;}
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
 unsigned char *code=map(0x594000,4096);assert(!fseek(f,0x594000,SEEK_SET));assert(fread(code,1,4096,f)==4096);fclose(f);
 unsigned char *stubs=map(0x5ac000,8192),*global=map(0x2606000,4096);
 /* Substitute only lock/unlock and address lookup; original state query runs intact. */
 memcpy(stubs+0x13e0,"\x31\xc0\xc3",3);
 memcpy(stubs+0x1460,"\x89\xf8\xc3",3);
 unsigned char *lookup=stubs+0x350;lookup[0]=0x48;lookup[1]=0xb8;u64(lookup+2,(uintptr_t)node);lookup[10]=0xc3;
 assert(!mprotect(code,4096,5));
 int (*query)(uint64_t)=(void*)0x594bf0;unsigned checks=0;
 for(unsigned kind=0;kind<3;kind++)for(unsigned service=0;service<5;service++)for(unsigned phase=0;phase<7;phase++){
  const unsigned services[]={0,4,8,0x38,0x40};
  memset(node,0,sizeof(node));memset(record,0,sizeof(record));memset(context,0,sizeof(context));
  u64(global+0x98,(uintptr_t)context);u32(context+8,8);u64(node+0x638,(uintptr_t)record);u32(record+0x20,services[service]);
  if(kind){record[0x2f]=2;record[0x28]=0x4c;record[0x29]=5;record[0x2a]=kind==1?0xc4:0xc5;record[0x2b]=5;}
  u32(node+0x18,phase==0?0:phase==1?0x4000040:0x4000000);
  node[0x12dc]=phase>=3?(phase-2)*4:0;node[0x642]=phase==3;node[0x643]=phase==4;
  int expected=phase==0?2:phase==1?4:kind==1?(phase==3 || phase==4?5:6):services[service]&12?(node[0x12dc]&12?5:6):3;
  assert(!mprotect(stubs,8192,5));int got=query(0x112233445566);assert(got==expected);checks++;
 }
 u32(context+8,0);assert(query(0)==1);checks++;
 u64(global+0x98,0);assert((uint32_t)query(0)==0x802f3201);checks++;
 printf("state_query_checks=%u passed; generic_audio_connected_without_stream=6\n",checks);
}
