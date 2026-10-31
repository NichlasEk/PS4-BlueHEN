"""Exercise native controller recovery and journal ordering with a fake kernel."""
from pathlib import Path
import subprocess
import tempfile
import unittest

class Automatic(unittest.TestCase):
    def test_lifecycle_and_recovery(self):
        source = r'''
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#define BH_AUTOMATIC_HOST_TEST
struct bh_request;
static int auto_mock_call(struct bh_request *);
static void *resolve(const char *,const char *);
#include "bt_automatic.h"
static int boot_missing;
static uint64_t boot_sec=1000, kernel_target;
static int installed,enabled,prepares,inspect_calls,fail_enable;
static int mock_sysctl(const int *m,unsigned n,void *out,size_t *size,const void *in,size_t len){assert(n==2 && m[0]==1 && m[1]==21 && *size==16 && !in && !len);if(boot_missing){errno=ENOENT;return -1;}uint64_t v[2]={boot_sec,123};memcpy(out,v,16);return 0;}
static uint64_t mock_frequency(void){return 1600000000;}
static void *resolve(const char *m,const char *n){(void)m;return !strcmp(n,"sysctl")?(void*)mock_sysctl:!strcmp(n,"sceKernelGetTscFrequency")?(void*)mock_frequency:NULL;}
int32_t sceKernelMmap(void *a,size_t n,int32_t p,int32_t flags,int32_t fd,off_t off,void **out){assert(!a && n==0x4000 && p==3 && flags==0x1002 && fd==-1 && off==0);*out=calloc(1,n);return *out?0:-1;}
int32_t sceKernelMprotect(const void *a,size_t n,int p){assert(a && n==0x4000 && p==5);return 0;}
int32_t sceKernelMunmap(void *a,size_t n){assert(n==0x4000);free(a);return 0;}
static int auto_mock_call(struct bh_request *r){
 switch(r->command){
 case BH_PREFLIGHT:break;
 case BH_PREPARE:{
   FILE *f=fopen(AUTO_JOURNAL,"rb");assert(f);struct auto_journal j;assert(fread(&j,1,sizeof(j),f)==sizeof(j));fclose(f);
   assert(!j.ready && !j.resident && j.owner==r->owner); /* Durable intent precedes allocation. */
   r->resident=0xffffa00000004000ULL;r->base=0xffffffff80000000ULL;kernel_target=r->target;prepares++;break;}
 case BH_INSPECT:inspect_calls++;r->target=kernel_target;break;
 case BH_RETARGET:assert(!installed && !enabled);kernel_target=r->target;break;
 case BH_INSTALL:assert(!installed && !enabled);installed=1;break;
 case BH_ENABLE:assert(installed);if(fail_enable)return 5;enabled=1;break;
 case BH_PULSE:assert(installed && enabled);break;
 case BH_DISABLE:enabled=0;break;
 case BH_REMOVE:assert(!enabled);installed=0;break;
 default:assert(0);
 }
 r->installed=installed;r->enabled=enabled;r->complete=BH_MAGIC;r->result=0;r->cpu_mask=r->serial_mask=0xff;r->generation++;return 0;
}
static void reset_app(void){if(auto_code)free(auto_code);auto_code=NULL;auto_ready=auto_active=auto_failed=0;auto_last_pulse=0;memset(&auto_request,0,sizeof(auto_request));memset(&auto_disk,0,sizeof(auto_disk));}
int main(void){FILE *f=tmpfile();assert(f);
 boot_missing=1;assert(auto_init(f)<0 && !auto_ready && !auto_code && !prepares);reset_app();boot_missing=0;
 assert(auto_init(f)==0 && !auto_request.resident);assert(!auto_begin(f,0x123456));assert(installed && enabled && auto_active && prepares==1);
 struct stat st;assert(!stat(AUTO_JOURNAL,&st) && (st.st_mode&0777)==0600);
 auto_last_pulse=0;assert(!auto_pulse(f));assert(!auto_finish(f) && !installed && !enabled);
 assert(!auto_begin(f,0xabcdef) && prepares==1 && kernel_target==0xabcdef);
 /* Simulated app crash: recover the retained object without allocating again. */
 reset_app();assert(!auto_init(f) && !installed && !enabled && prepares==1);
 fail_enable=1;assert(auto_begin(f,0x123456)<0 && !installed && !enabled);fail_enable=0;
 auto_shutdown(f);reset_app();assert(!auto_init(f));
 auto_disk.ready=0;assert(!auto_save());reset_app();int old_calls=inspect_calls;
 assert(auto_init(f)<0 && inspect_calls==old_calls);reset_app();
 /* New boot discards pointers before any kernel inspection. */
 boot_sec++;installed=enabled=0;assert(!auto_init(f) && !auto_request.resident && inspect_calls==old_calls);
 assert(!auto_begin(f,0x123456) && prepares==2);auto_shutdown(f);reset_app();
 FILE *j=fopen(AUTO_JOURNAL,"r+b");assert(j);fputc(0,j);fclose(j);
 old_calls=inspect_calls;assert(auto_init(f)<0 && inspect_calls==old_calls);reset_app();
 fclose(f);puts("automatic journal, crash recovery, failed enable, retarget, boot invalidation: passed");return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            (p/'bt-auto-blob.h').write_text('static const unsigned char bh_auto_blob[32]={0};\n')
            (p/'test.c').write_text(source)
            exe=p/'test'
            subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',
                            '-I',str(Path(__file__).parent),'-I',tmp,
                            '-DAUTO_JOURNAL="'+str(p/'state')+'"',str(p/'test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,timeout=5)
