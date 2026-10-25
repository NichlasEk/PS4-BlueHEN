/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bounded own-descriptor inquiry. No pairing, connection or audio. */
#define BLUEHEN_MODULE_HOST_TEST
#include "../module-probe/probe.c"
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "bt_registered.h"

struct bt_event { uint8_t type,status,reserved[2]; uint32_t value; uint64_t address; };
struct bt_kevent { uint64_t ident; int16_t filter; uint16_t flags; uint32_t fflags; int64_t data; void *udata; };
struct bt_timeout { int64_t sec,nsec; };
_Static_assert(sizeof(struct bt_event)==16,"event ABI");
_Static_assert(sizeof(struct bt_kevent)==32,"kevent ABI");
static int (*bt_kqueue)(void);
static int (*bt_kevent)(int,const struct bt_kevent *,int,struct bt_kevent *,int,const struct bt_timeout *);
static FILE *diag_log;

/* Poll only our descriptor, with the native zero-timeout kevent ABI. */
static int next_event(int queue,int fd,struct bt_event *event)
{
    struct bt_kevent ready={0};
    const struct bt_timeout timeout={0};
    int rc=bt_kevent(queue,NULL,0,&ready,1,&timeout);
    if(rc<0 && errno==EINTR) return 0;
    if(rc<=0) return rc;
    if(rc!=1 || ready.ident!=(uint64_t)fd || ready.filter!=-1 ||
       (ready.flags&(0x4000u|0x8000u))) return -2;
    struct { uint64_t before; struct bt_event event; uint64_t after; } guarded={BT_CANARY,{0},BT_CANARY};
    ssize_t n=read(fd,&guarded.event,sizeof(guarded.event));
    if(n<0 && (errno==EAGAIN || errno==EINTR)) return 0;
    if(n!=sizeof(guarded.event) || guarded.before!=BT_CANARY || guarded.after!=BT_CANARY) return -3;
    *event=guarded.event;
    return 1;
}

static int inquiry(FILE *f)
{
    int status=20,fd=-1,queue=-1,attached=0,started=0;
    unsigned events=0,results=0,unique=0; uint64_t addresses[32]={0};
    fd=open("/dev/bt",O_RDWR|O_NONBLOCK);
    fprintf(f,"BT_OPEN ok=%d errno=%d\n",fd>=0,fd<0?errno:0);
    if(fd<0) goto cleanup;
    /* Same eight-byte subscription request as ShellUI. Ownership/coexistence
     * still requires physical verification; never use ShellUI's descriptor. */
    const uint64_t subscription=0xfe;
    ssize_t n=write(fd,&subscription,sizeof(subscription));
    fprintf(f,"BT_SUBSCRIBE bytes=%lld errno=%d\n",(long long)n,n<0?errno:0);
    if(n!=sizeof(subscription)) {status=21;goto cleanup;}
    queue=bt_kqueue();
    if(queue<0) {status=22;goto cleanup;}
    struct bt_kevent change={.ident=(uint64_t)fd,.filter=-1,.flags=1};
    int rc=bt_kevent(queue,&change,1,NULL,0,NULL);
    fprintf(f,"BT_QUEUE_ADD result=%d\n",rc);
    if(rc<0) {status=23;goto cleanup;}
    attached=1;
    /* Do not issue stop for someone else's discovery operation. Reject
     * inquiry traffic during the passive phase. This cannot prove exclusivity. */
    for(unsigned tick=0;tick<40;tick++) {
        struct bt_event event={0}; rc=next_event(queue,fd,&event);
        if(rc<0) {status=24;goto cleanup;}
        if(rc>0) {
            fprintf(f,"BT_PASSIVE_EVENT type=%u status=%u\n",event.type,event.status);
            if(event.type==1 || event.type==2) {status=25;goto cleanup;}
        }
        usleep(50000);
    }
    fprintf(f,"BT_INQUIRY_BEGIN limit_ticks=300 tick_us=50000\n");
    errno=0;rc=ioctl(fd,0x20046204UL,0);
    fprintf(f,"BT_INQUIRY_START result=%d errno=%d\n",rc,rc<0?errno:0);
    if(rc!=0) {status=26;goto cleanup;}
    started=1;status=0;
    for(unsigned tick=0;tick<300;tick++) {
        struct bt_event event={0};rc=next_event(queue,fd,&event);
        if(rc<0) {status=27;break;}
        if(rc>0) {
            if(++events>128) {status=28;break;}
            if(event.type==1) {
                results++;
                unsigned slot=0;
                while(slot<unique && addresses[slot]!=event.address) slot++;
                if(slot==unique && event.address && unique<32) addresses[unique++]=event.address;
                fprintf(f,"BT_INQUIRY_RESULT slot=%u status=%u value=0x%08x address_present=%d\n",slot,event.status,event.value,event.address!=0);
            } else {
                fprintf(f,"BT_EVENT type=%u status=%u\n",event.type,event.status);
            }
            if(event.type==2) {
                fprintf(f,"BT_INQUIRY_STOP_EVENT status=%u\n",event.status);
                if(event.status) status=29;
                break;
            }
        }
        usleep(50000);
    }
 cleanup:
    if(started) {
        errno=0;int stop_rc=ioctl(fd,0x20046205UL,0);
        fprintf(f,"BT_INQUIRY_STOP result=%d errno=%d\n",stop_rc,stop_rc<0?errno:0);
        if(stop_rc!=0) status=30;
    }
    if(attached) {
        struct bt_kevent change={.ident=(uint64_t)fd,.filter=-1,.flags=2};
        int del_rc=bt_kevent(queue,&change,1,NULL,0,NULL);
        fprintf(f,"BT_QUEUE_DELETE result=%d\n",del_rc);
        if(del_rc<0 && status==0) status=31;
    }
    if(queue>=0) {int rc=close(queue);fprintf(f,"BT_QUEUE_CLOSE result=%d\n",rc);if(rc && status==0) status=32;}
    if(fd>=0) {int rc=close(fd);fprintf(f,"BT_CLOSE result=%d\n",rc);if(rc && status==0) status=33;}
    memset(addresses,0,sizeof(addresses));
    fprintf(f,"BT_SUMMARY events=%u inquiry_results=%u unique=%u NO_NAMES_OR_ADDRESSES\n",events,results,unique);
    return status;
}

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
    FILE *f=fopen("/data/bluehen/service-scan-v11.log","a");
    if (!f) return 1;
    setvbuf(f,NULL,_IONBF,0);
    diag_log=f;
    struct sw_version version={.size=sizeof(version)};
    int rc=sceKernelGetSystemSwVersion(&version);
    fprintf(f,"SERVICE_SCAN_V11 pid=%d firmware_rc=0x%08x firmware=0x%08x\n",pid,(unsigned)rc,version.version);
    if (rc || (version.version&0xffff0000u)!=0x09600000u) goto done;
    inventory();
    const char *kernels[]={"libkernel.sprx","libkernel_sys.sprx","libkernel_web.sprx","libkernel","libkernel_sys","libkernel_web"};
    int (*auth)(int,void *)=NULL; int (*parent)(void)=NULL;
    for (unsigned i=0;i<sizeof(kernels)/sizeof(kernels[0]);i++) {
        if (!auth) auth=resolve(kernels[i],"get_authinfo");
        if (!parent) parent=resolve(kernels[i],"getppid");
        if (!bt_kqueue) bt_kqueue=resolve(kernels[i],"kqueue");
        if (!bt_kevent) bt_kevent=resolve(kernels[i],"kevent");
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
    if (!exit_app || !bt_kqueue || !bt_kevent) {status=8;goto done;}
    fprintf(f,"QUEUE_READY kqueue=%p kevent=%p\n",(void *)bt_kqueue,(void *)bt_kevent);
    fprintf(f,"READY pid=%d auth=%p exit=%p NO_DAEMON_REGISTER\n",pid,(void *)auth,(void *)exit_app);
    unsigned waited;
    for (waited=0;waited<60;waited++) {
        if (permit("/data/bluehen/service-scan-v11.allow",pid)) break;
        sleep(1);
    }
    if (waited==60) {fprintf(f,"REFUSED permission timeout\n");goto done;}
    status=inquiry(f);
 done:
    fprintf(f,"RESULT result=%d NO_AUDIO NO_PAIR NO_CONNECT NO_DISCONNECT\n",status);
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
    fprintf(f,"END result=%d NO_AUDIO NO_PAIR NO_CONNECT NO_DISCONNECT NO_CREDENTIAL_CHANGES\n",status);
    fclose(f);return status;
}
