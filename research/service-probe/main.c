/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bounded, lifecycle-only daemon-category experiment. No audio or Bluetooth. */
#define BLUEHEN_MODULE_HOST_TEST
#include "../module-probe/probe.c"
#include <stdint.h>

static void *resolve(const char *module, const char *symbol)
{
    uint32_t handles[256]; size_t count=0;
    if (sceKernelGetModuleList(handles,256,&count) || count>256) return NULL;
    for (size_t i=0;i<count;i++) {
        struct module_info info={.size=sizeof(info)};
        if (sceKernelGetModuleInfo(handles[i],&info)) continue;
        if (!strcmp(info.name,module)) {
            void *p=NULL;
            if (!sceKernelDlsym(handles[i],symbol,&p)) return p;
        }
    }
    return NULL;
}
static int permit(const char *path,int pid)
{
    FILE *f=fopen(path,"r"); if (!f) return 0;
    int candidate=-1; int n=fscanf(f,"%d",&candidate); fclose(f);
    return n==1 && candidate==pid;
}
int main(void)
{
    int pid=(int)getpid(),status=1;
    FILE *f=fopen("/data/bluehen/service-v1.log","a");
    if (!f) return 1;
    setvbuf(f,NULL,_IONBF,0);
    struct sw_version version={.size=sizeof(version)};
    int rc=sceKernelGetSystemSwVersion(&version);
    fprintf(f,"SERVICE_V1 pid=%d firmware_rc=0x%08x firmware=0x%08x\n",pid,(unsigned)rc,version.version);
    if (rc || (version.version&0xffff0000u)!=0x09600000u) goto done;
    const char *kernels[]={"libkernel.sprx","libkernel_sys.sprx"};
    int (*auth)(int,void *)=NULL; int (*parent)(void)=NULL;
    for (unsigned i=0;i<2;i++) {
        if (!auth) auth=resolve(kernels[i],"get_authinfo");
        if (!parent) parent=resolve(kernels[i],"getppid");
    }
    int (*reg)(void)=resolve("libSceSystemService.sprx","sceSystemServiceRegisterDaemon");
    if (!auth || !parent || !reg) { fprintf(f,"REFUSED unresolved lifecycle exports\n"); goto done; }
    uint64_t info[32]={0}; rc=auth(pid,info);
    fprintf(f,"IDENTITY pid=%d ppid=%d auth_rc=%d system_bit=%u\n",pid,parent(),rc,rc==0?(unsigned)((info[1]>>62)&1):0);
    memset(info,0,sizeof(info));
    if (rc) goto done;
    fprintf(f,"READY pid=%d register=%p auth=%p\n",pid,(void *)reg,(void *)auth);
    unsigned waited;
    for (waited=0;waited<60;waited++) {
        if (permit("/data/bluehen/service-v1.allow",pid)) break;
        sleep(1);
    }
    if (waited==60) {fprintf(f,"REFUSED permission timeout\n");goto done;}
    fprintf(f,"REGISTER_BEGIN\n"); rc=reg();
    fprintf(f,"REGISTER_END result=0x%08x\n",(unsigned)rc);
    if (rc) {status=2;goto done;}
    for (unsigned tick=0;tick<60;tick++) {
        fprintf(f,"HEARTBEAT pid=%d tick=%u ppid=%d\n",pid,tick,parent());
        if (permit("/data/bluehen/service-v1.stop",pid)) {fprintf(f,"STOP accepted\n");status=0;goto done;}
        sleep(1);
    }
    fprintf(f,"STOP timeout\n");status=3;
 done:
    fprintf(f,"END result=%d NO_AUDIO NO_BLUETOOTH NO_CREDENTIAL_CHANGES\n",status);
    fclose(f);return status;
}
