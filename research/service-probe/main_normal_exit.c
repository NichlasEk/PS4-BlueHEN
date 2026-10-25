/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bounded, lifecycle-only normal-app category experiment. No audio or Bluetooth. */
#define BLUEHEN_MODULE_HOST_TEST
#include "../module-probe/probe.c"
#include <stdint.h>

static FILE *diag_log;

static void inventory(void)
{
    uint32_t handles[256]; size_t count=0;
    int rc=sceKernelGetModuleList(handles,256,&count);
    fprintf(diag_log,"INVENTORY rc=%d count=%zu\n",rc,count);
    if(rc || count>256) return;
    for(size_t i=0;i<count;i++) {
        struct module_info info={.size=sizeof(info)};
        rc=sceKernelGetModuleInfo(handles[i],&info);
        if(!rc) fprintf(diag_log,"MODULE handle=%u name=%.256s\n",handles[i],info.name);
    }
}

static void *resolve(const char *module, const char *symbol)
{
    uint32_t handles[256]; size_t count=0;
    if (sceKernelGetModuleList(handles,256,&count) || count>256) return NULL;
    for (size_t i=0;i<count;i++) {
        struct module_info info={.size=sizeof(info)};
        if (sceKernelGetModuleInfo(handles[i],&info)) continue;
        if (!strcmp(info.name,module)) {
            void *p=NULL;
            int rc=sceKernelDlsym(handles[i],symbol,&p);
            fprintf(diag_log,"LOOKUP module=%s symbol=%s rc=0x%08x resolved=%d\n",module,symbol,(unsigned)rc,p!=NULL);
            if (!rc) return p;
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
    int pid=(int)getpid(),status=1,system_owned=0;
    int (*exit_app)(const char *,const char **)=NULL;
    FILE *f=fopen("/data/bluehen/service-exit-v9.log","a");
    if (!f) return 1;
    setvbuf(f,NULL,_IONBF,0);
    diag_log=f;
    struct sw_version version={.size=sizeof(version)};
    int rc=sceKernelGetSystemSwVersion(&version);
    fprintf(f,"SERVICE_EXIT_V9 pid=%d firmware_rc=0x%08x firmware=0x%08x\n",pid,(unsigned)rc,version.version);
    if (rc || (version.version&0xffff0000u)!=0x09600000u) goto done;
    inventory();
    const char *kernels[]={"libkernel.sprx","libkernel_sys.sprx","libkernel_web.sprx","libkernel","libkernel_sys","libkernel_web"};
    int (*auth)(int,void *)=NULL; int (*parent)(void)=NULL;
    for (unsigned i=0;i<sizeof(kernels)/sizeof(kernels[0]);i++) {
        if (!auth) auth=resolve(kernels[i],"get_authinfo");
        if (!parent) parent=resolve(kernels[i],"getppid");
    }
    if (!auth) {fprintf(f,"REFUSED unresolved get_authinfo\n");goto done;}
    uint64_t info[32]={0}; rc=auth(pid,info);
    fprintf(f,"IDENTITY pid=%d ppid=%d auth_rc=%d system_bit=%u\n",pid,parent?parent():-1,rc,rc==0?(unsigned)((info[1]>>62)&1):0);
    int profile_match=rc==0 && !memcmp(info,bluehen_expected_auth,sizeof(bluehen_expected_auth));
    fprintf(f,"AUTH_PROFILE_MATCH value=%d\n",profile_match);
    memset(info,0,sizeof(info));
    if (!profile_match) {status=4;goto done;}
    /* OpenOrbis _types/sysmodule.h: INTERNAL_SYSTEM_SERVICE = 0x80000010.
     * Acquire only in this process after the complete auth profile matches. */
    fprintf(f,"SYSTEM_LOAD_BEGIN id=0x80000010\n");
    rc=sceSysmoduleLoadModuleInternal(0x80000010u);
    fprintf(f,"SYSTEM_LOAD_END rc=0x%08x\n",(unsigned)rc);
    if (rc) {status=6;goto done;}
    system_owned=1;
    inventory();
    exit_app=resolve("libSceSystemService.sprx","sceSystemServiceLoadExec");
    if (!exit_app || !parent) {status=8;goto done;}
    fprintf(f,"READY pid=%d auth=%p exit=%p NO_DAEMON_REGISTER\n",pid,(void *)auth,(void *)exit_app);
    unsigned waited;
    for (waited=0;waited<60;waited++) {
        if (permit("/data/bluehen/service-exit-v9.allow",pid)) break;
        sleep(1);
    }
    if (waited==60) {fprintf(f,"REFUSED permission timeout\n");goto done;}
    for (unsigned tick=0;tick<60;tick++) {
        fprintf(f,"HEARTBEAT pid=%d tick=%u ppid=%d\n",pid,tick,parent());
        if (permit("/data/bluehen/service-exit-v9.stop",pid)) {fprintf(f,"STOP accepted\n");status=0;goto done;}
        sleep(1);
    }
    fprintf(f,"STOP timeout\n");status=3;
 done:
    if(status==0 && exit_app) {
        /* Keep SystemService loaded until the system completes application exit. */
        fprintf(f,"EXIT_SYSTEM_BEGIN status=%d\n",status);
        rc=exit_app("exit",NULL);
        fprintf(f,"EXIT_SYSTEM_RETURN result=0x%08x\n",(unsigned)rc);
        sleep(3);
        fprintf(f,"EXIT_FALLBACK process still running\n");
        status=9;
    }
    if (system_owned) {
        fprintf(f,"SYSTEM_UNLOAD_BEGIN\n");
        rc=sceSysmoduleUnloadModuleInternal(0x80000010u);
        fprintf(f,"SYSTEM_UNLOAD_END rc=0x%08x\n",(unsigned)rc);
        if (rc) status=7;
    }
    fprintf(f,"END result=%d NO_AUDIO NO_BLUETOOTH NO_CREDENTIAL_CHANGES\n",status);
    fclose(f);return status;
}
