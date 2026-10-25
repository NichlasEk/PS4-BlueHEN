/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bounded, lifecycle-only daemon-category experiment. One registered Bluetooth metadata query; no radio-control operations. */
#define BLUEHEN_MODULE_HOST_TEST
#include "../module-probe/probe.c"
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "bt_registered.h"

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
    int (*exit_app)(const char *,const char **)=NULL;
    int pid=(int)getpid(),status=1,system_owned=0;
    FILE *f=fopen("/data/bluehen/service-bt-v10.log","a");
    if (!f) return 1;
    setvbuf(f,NULL,_IONBF,0);
    diag_log=f;
    struct sw_version version={.size=sizeof(version)};
    int rc=sceKernelGetSystemSwVersion(&version);
    fprintf(f,"SERVICE_BT_V10 pid=%d firmware_rc=0x%08x firmware=0x%08x\n",pid,(unsigned)rc,version.version);
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
    if (!exit_app) {status=8;goto done;}
    fprintf(f,"READY pid=%d auth=%p exit=%p NO_DAEMON_REGISTER\n",pid,(void *)auth,(void *)exit_app);
    unsigned waited;
    for (waited=0;waited<60;waited++) {
        if (permit("/data/bluehen/service-bt-v10.allow",pid)) break;
        sleep(1);
    }
    if (waited==60) {fprintf(f,"REFUSED permission timeout\n");goto done;}
    struct bt_guarded_records records={.before=BT_CANARY,.after=BT_CANARY};
    struct bt_registered_query query={.address=0,.records=records.items,.capacity=32};
    fprintf(f,"BT_OPEN_BEGIN\n");
    int fd=open("/dev/bt",O_RDWR);
    fprintf(f,"BT_OPEN_END ok=%d errno=%d\n",fd>=0,fd<0?errno:0);
    if(fd<0) {status=12;goto done;}
    fprintf(f,"BT_QUERY_BEGIN capacity=32 record_bytes=256\n");
    errno=0;
    int result=ioctl(fd,0x80186201UL,&query);
    int query_errno=errno;
    int close_rc=close(fd);
    int guards=records.before==BT_CANARY && records.after==BT_CANARY;
    fprintf(f,"BT_QUERY_END result=%d errno=%d guards=%d\n",result,query_errno,guards);
    fprintf(f,"BT_CLOSE_END result=%d\n",close_rc);
    if(result<0 || !guards || close_rc) {status=13;goto done;}
    unsigned present=0;
    for(unsigned i=0;i<32;i++) {
        const struct bt_registered_record *record=&records.items[i];
        if(!record->address) continue;
        present++;
        fprintf(f,"BT_RECORD slot=%u cod=0x%08x services=0x%08x flags=0x%08x a2dp_sink=%u a2dp_source=%u hid_device=%u\n",
            i,record->cod,record->services,record->flags,
            (record->services&8u)!=0,(record->services&4u)!=0,(record->services&0x40u)!=0);
    }
    fprintf(f,"BT_RECORDS_PRESENT count=%u NO_NAMES_OR_ADDRESSES\n",present);
    memset(&records,0,sizeof(records));
    status=0;
 done:
    fprintf(f,"RESULT result=%d NO_AUDIO NO_SCAN NO_PAIR NO_CONNECT NO_DISCONNECT\n",status);
    if (system_owned && exit_app) {
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
    fprintf(f,"END result=%d NO_AUDIO NO_SCAN NO_PAIR NO_CONNECT NO_DISCONNECT NO_CREDENTIAL_CHANGES\n",status);
    fclose(f);return status;
}
