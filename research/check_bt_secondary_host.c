/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
/* Execute only the second admission predicate, after its timer update.
 * Both destinations are replaced with local returns. No console access. */
__asm__(".text\n.global secondary_gate\nsecondary_gate:\n"
        "push %rbx\nmov %rdi,%rbx\nmov $0x59f96e,%eax\ncall *%rax\npop %rbx\nret\n");
extern int secondary_gate(void *record);
__asm__(".text\n.global secondary_candidate\nsecondary_candidate:\n"
        "push %r11\npush %r10\nmov (%rbx),%r11\n"
        "shl $16,%r11\nshr $16,%r11\nmovabs $0x112233445566,%r10\n"
        "cmp %r10,%r11\njne 2f\nmov 8(%rbx),%r11d\n"
        "and $0x1f00,%r11d\ncmp $0x400,%r11d\njne 2f\n"
        "testl $8,0x20(%rbx)\njz 2f\ntestl $0x40,0x20(%rbx)\njnz 2f\n"
        "pop %r10\npop %r11\njmp 0x59f998\n"
        "2: pop %r10\npop %r11\ncmpb $2,0x2f(%rbx)\n"
        "jne 3f\njmp 0x59f974\n3: jmp 0x59f9d6\n");
extern void secondary_candidate(void);
static void put32(void *p,uint32_t v){memcpy(p,&v,4);}
struct scenario {const char *name;uint32_t services,cod;uint16_t vid,pid;uint8_t source;int stock,extra;};
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    unsigned char *code=mmap((void*)0x59f000,4096,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(code==(void*)0x59f000);assert(!fseek(f,0x59f000,SEEK_SET));
    assert(fread(code,1,4096,f)==4096);fclose(f);
    const unsigned char signature[]={0x80,0x7b,0x2f,0x02,0x75,0x62};
    assert(!memcmp(code+0x96e,signature,6));
    const unsigned exits[]={0x998,0x9d6};
    for(unsigned i=0;i<2;i++){code[exits[i]]=0xb8;put32(code+exits[i]+1,i);code[exits[i]+5]=0xc3;}
    assert(!mprotect(code,4096,PROT_READ|PROT_EXEC));
    const struct scenario cases[]={
      {"hp206",0x38,0x240404,0,0,0,1,1},
      {"generic_sink",8,0x240404,0,0,0,1,1},
      {"hid",0x40,0x2508,0,0,0,1,0},
      {"mixed",0x48,0x240404,0,0,0,1,0},
      {"wrong_class",8,0x2508,0,0,0,1,0},
      {"source_only",4,0x240404,0,0,0,1,0},
      {"empty",0,0x240404,0,0,0,1,0},
      {"stock_044f",0x40,0x2508,0x044f,0x1000,2,0,0},
      {"stock_06f8_a",0x40,0x2508,0x06f8,0xe051,2,0,0},
      {"stock_06f8_b",0x40,0x2508,0x06f8,0xe052,2,0,0},
      {"wrong_product",0x40,0x2508,0x06f8,0xe053,2,1,0},
      {"wrong_id_source",0x40,0x2508,0x044f,0x1000,1,1,0},
      {"sink_source2",8,0x240404,0,0,2,1,1}};
    for(int candidate=0;candidate<2;candidate++){
      if(candidate){
        assert(!mprotect(code,4096,PROT_READ|PROT_WRITE));
        intptr_t delta=(intptr_t)secondary_candidate-(0x59f96e + 5);
        assert(delta>=INT32_MIN && delta<=INT32_MAX);
        code[0x96e]=0xe9;put32(code+0x96f,(uint32_t)(int32_t)delta);code[0x973]=0x90;
        assert(!mprotect(code,4096,PROT_READ|PROT_EXEC));
      }
      for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++)for(int selected=0;selected<3;selected++){
        const struct scenario *t=&cases[i];unsigned char rec[0x40]={0};
        uint64_t addr=selected?UINT64_C(0x112233445566):UINT64_C(0xabcdef123456);
        if(selected==2)addr|=UINT64_C(0xabcd000000000000);
        memcpy(rec,&addr,8);put32(rec+8,t->cod);put32(rec+0x20,t->services);
        memcpy(rec+0x28,&t->vid,2);memcpy(rec+0x2a,&t->pid,2);rec[0x2f]=t->source;
        int expected=candidate && selected && t->extra?0:t->stock;
        int got=secondary_gate(rec);assert(got==expected);
        printf("candidate=%d selected=%d %s %d\n",candidate,selected,t->name,got);
      }
    }
    return 0;
}
