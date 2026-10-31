/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Native own-process controller. No host/debugger or remote permission file. */
#define BH_AUTOMATIC
#include "../bt-hook-layout.h"
#include "../bt-auto-sha256.h"
#include "bt-auto-blob.h"
#include <sys/types.h>
int32_t sceKernelMmap(void *,size_t,int32_t,int32_t,int32_t,off_t,void **);
int32_t sceKernelMprotect(const void *,size_t,int);
int32_t sceKernelMunmap(void *,size_t);
#ifndef AUTO_JOURNAL
#define AUTO_JOURNAL "/data/bluehen/automatic-v1.state"
#endif
struct auto_journal {
 uint64_t magic,boot_sec,boot_usec,ready,base,resident,owner,target,tsc_hz;
 unsigned char blob_sha[32],digest[32];
};
static struct bh_request auto_request;
static struct auto_journal auto_disk;
static void *auto_code;
static int auto_ready,auto_active,auto_failed;
static uint64_t auto_last_pulse;
static uint64_t auto_ticks(void){uint32_t a,d;__asm__ volatile("rdtsc":"=a"(a),"=d"(d));return ((uint64_t)d<<32)|a;}
static int auto_save(void)
{
 auto_disk.base=auto_request.base;auto_disk.resident=auto_request.resident;
 auto_disk.owner=auto_request.owner;auto_disk.target=auto_request.target;auto_disk.tsc_hz=auto_request.tsc_hz;
 bh_hash(&auto_disk,offsetof(struct auto_journal,digest),auto_disk.digest);
 FILE *f=fopen(AUTO_JOURNAL ".tmp","wb");if(!f)return -1;
 int ok=fchmod(fileno(f),0600)==0 && fwrite(&auto_disk,1,sizeof(auto_disk),f)==sizeof(auto_disk) && fflush(f)==0 && fsync(fileno(f))==0;
 if(fclose(f))ok=0;
 if(!ok || rename(AUTO_JOURNAL ".tmp",AUTO_JOURNAL))return -1;
 return 0;
}
static int auto_command(FILE *f,unsigned command)
{
 auto_request.command=command;auto_request.complete=0;auto_request.result=UINT64_MAX;
 #ifdef BH_AUTOMATIC_HOST_TEST
 int rc=auto_mock_call(&auto_request);
#else
 int (*call)(struct bh_request*)=auto_code;int rc=call(&auto_request);
#endif
 int ok=rc==0 && auto_request.complete==BH_MAGIC && auto_request.result==0;
 if(command==BH_INSTALL || command==BH_ENABLE || command==BH_DISABLE || command==BH_REMOVE)
  ok=ok && auto_request.cpu_mask==0xff && auto_request.serial_mask==0xff;
 if(command!=BH_PULSE || !ok)fprintf(f,"AUTO_COMMAND command=%u return=%d complete=%d result=%llu installed=%llu enabled=%llu generation=%llu\n",command,rc,auto_request.complete==BH_MAGIC,(unsigned long long)auto_request.result,(unsigned long long)auto_request.installed,(unsigned long long)auto_request.enabled,(unsigned long long)auto_request.generation);
 if(!ok){auto_failed=1;return -1;}
 return 0;
}
static int auto_finish(FILE *f)
{
 auto_active=0;
 if(!auto_ready || !auto_request.resident)return 0;
 /* Observe before recovery. Never blindly resubmit an uncertain command. */
 if(auto_command(f,BH_INSPECT))return -1;
 if(auto_request.enabled && auto_command(f,BH_DISABLE))return -1;
 if(auto_request.installed && auto_command(f,BH_REMOVE))return -1;
 if(auto_command(f,BH_INSPECT) || auto_request.installed || auto_request.enabled)return -1;
 fprintf(f,"AUTO_RESTORED primary_hits=%llu primary_allowed=%llu secondary_hits=%llu secondary_allowed=%llu\n",(unsigned long long)auto_request.primary_hits,(unsigned long long)auto_request.primary_allowed,(unsigned long long)auto_request.secondary_hits,(unsigned long long)auto_request.secondary_allowed);
 return auto_save();
}
static int auto_init(FILE *f)
{
 int (*query)(const int *,unsigned,void *,size_t *,const void *,size_t)=NULL;
 uint64_t (*frequency)(void)=NULL;
 const char *names[]={"libkernel.sprx","libkernel_sys.sprx","libkernel_web.sprx","libkernel","libkernel_sys","libkernel_web"};
 for(unsigned i=0;i<6;i++){if(!query)query=resolve(names[i],"sysctl");if(!frequency)frequency=resolve(names[i],"sceKernelGetTscFrequency");}
 if(!query || !frequency){fprintf(f,"AUTO_INIT_ERROR stage=exports\n");return -1;}
 uint64_t boot[2]={0};size_t size=sizeof(boot);int mib[2]={1,21};
 int boot_rc=query(mib,2,boot,&size,NULL,0);
 if(boot_rc || size!=16 || !boot[0] || boot[1]>=1000000){fprintf(f,"AUTO_INIT_ERROR stage=boot_identity rc=%d size=%zu valid=%d\n",boot_rc,size,boot[0]!=0 && boot[1]<1000000);return -1;}
 uint64_t hz=frequency();if(hz<100000000 || hz>10000000000ULL)return -1;
 /* Invariant TSC is required for the resident lease across power states. */
 uint32_t a=0x80000000,b,c=0,d;
 __asm__ volatile("cpuid":"+a"(a),"=b"(b),"+c"(c),"=d"(d));if(a<0x80000007)return -1;
 a=0x80000007;c=0;__asm__ volatile("cpuid":"+a"(a),"=b"(b),"+c"(c),"=d"(d));if(!(d&(1u<<8)))return -1;
 unsigned char blob_sha[32];bh_hash(bh_auto_blob,sizeof(bh_auto_blob),blob_sha);
 auto_disk=(struct auto_journal){.magic=BH_MAGIC,.boot_sec=boot[0],.boot_usec=boot[1],.tsc_hz=hz};
 memcpy(auto_disk.blob_sha,blob_sha,32);
 FILE *file=fopen(AUTO_JOURNAL,"rb");
 if(file){struct auto_journal old={0};int valid=fread(&old,1,sizeof(old),file)==sizeof(old) && fgetc(file)==EOF;fclose(file);
  unsigned char sum[32];bh_hash(&old,offsetof(struct auto_journal,digest),sum);
  if(!valid || old.magic!=BH_MAGIC || memcmp(sum,old.digest,32)){fprintf(f,"AUTO_REFUSED journal integrity\n");return -1;}
  if(old.boot_sec==boot[0] && old.boot_usec==boot[1]){
   if(!old.ready || !old.resident || !old.owner || old.tsc_hz!=hz || memcmp(old.blob_sha,blob_sha,32)){fprintf(f,"AUTO_REFUSED unfinished or incompatible same-boot journal\n");return -1;}
   auto_disk=old;
  }
 }else if(errno!=ENOENT)return -1;
 auto_request=(struct bh_request){.base=auto_disk.base,.resident=auto_disk.resident,.owner=auto_disk.owner,.target=auto_disk.target,.tsc_hz=hz};
 if(!auto_request.owner){FILE *random=fopen("/dev/urandom","rb");if(!random)return -1;int ok=fread(&auto_request.owner,1,8,random)==8;fclose(random);if(!ok || !auto_request.owner)return -1;}
 /* Direct libkernel ABI uses FreeBSD MAP_ANON=0x1000, not libc Linux flags. */
 if(sceKernelMmap(NULL,0x4000,3,0x1002,-1,0,&auto_code))return -1;
 memcpy(auto_code,bh_auto_blob,sizeof(bh_auto_blob));
 if(sceKernelMprotect(auto_code,0x4000,5)){sceKernelMunmap(auto_code,0x4000);auto_code=NULL;return -1;}
 auto_ready=1;
 if(auto_request.resident && auto_finish(f))return -1;
 fprintf(f,"AUTO_READY native=1 lease_seconds=2 resident_reused=%d\n",auto_request.resident!=0);
 return 0;
}
static int auto_begin(FILE *f,uint64_t target)
{
 if(!auto_ready || auto_failed)return -1;
 if(auto_finish(f))return -1;
 auto_request.target=target;
 if(auto_request.resident){if(auto_command(f,BH_RETARGET) || auto_save())return -1;}
 if(auto_command(f,BH_PREFLIGHT))return -1;
 if(!auto_request.resident){
  auto_disk.ready=0;if(auto_save())return -1;
  if(auto_command(f,BH_PREPARE))return -1;
  auto_disk.ready=1;if(auto_save()){auto_failed=1;return -1;}
 }
 if(auto_command(f,BH_INSTALL) || auto_command(f,BH_ENABLE)){auto_finish(f);return -1;}
 auto_last_pulse=auto_ticks();auto_active=1;
 fprintf(f,"AUTO_ENABLED target_only=1 native=1\n");return 0;
}
static int auto_pulse(FILE *f)
{
 if(!auto_active)return 0;
 uint64_t now=auto_ticks();
 if(now-auto_last_pulse<auto_request.tsc_hz/4)return 0;
 if(auto_command(f,BH_PULSE)){auto_active=0;return -1;}
 auto_last_pulse=now;return 0;
}
static void auto_shutdown(FILE *f)
{
 if(!auto_ready){fprintf(f,"AUTO_SHUTDOWN initialized=0\n");return;}
 int restored=auto_finish(f)==0;
 fprintf(f,"AUTO_SHUTDOWN restored=%d\n",restored);
 if(auto_code){sceKernelMunmap(auto_code,0x4000);auto_code=NULL;}auto_ready=0;
}
