/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bounded, lifecycle-only daemon-category experiment. One silent PCM block and Vmic lifecycle; no Bluetooth or routing. */
#define BLUEHEN_MODULE_HOST_TEST
#include "../module-probe/probe.c"
#include <stdint.h>

static int valid_cleanup(int self, int permit_pid, unsigned handle, int32_t created)
{
    return self == permit_pid && (handle & 0xffff00fcu) == 0x40000u &&
           (created < 0 || handle == (unsigned)created);
}


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
    int pid=(int)getpid(),status=1,system_owned=0,audio_owned=0;
    FILE *f=fopen("/data/bluehen/service-v6.log","a");
    if (!f) return 1;
    setvbuf(f,NULL,_IONBF,0);
    diag_log=f;
    struct sw_version version={.size=sizeof(version)};
    int rc=sceKernelGetSystemSwVersion(&version);
    fprintf(f,"SERVICE_V6 pid=%d firmware_rc=0x%08x firmware=0x%08x\n",pid,(unsigned)rc,version.version);
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
    int (*reg)(void)=resolve("libSceSystemService.sprx","sceSystemServiceRegisterDaemon");
    if (!reg) reg=resolve("libSceSystemService","sceSystemServiceRegisterDaemon");
    if (!reg || !parent) {fprintf(f,"REFUSED lifecycle exports register=%d parent=%d\n",reg!=NULL,parent!=NULL);status=5;goto done;}
    fprintf(f,"READY pid=%d register=%p auth=%p\n",pid,(void *)reg,(void *)auth);
    unsigned waited;
    for (waited=0;waited<60;waited++) {
        if (permit("/data/bluehen/service-v6.allow",pid)) break;
        sleep(1);
    }
    if (waited==60) {fprintf(f,"REFUSED permission timeout\n");goto done;}
    fprintf(f,"REGISTER_BEGIN\n"); rc=reg();
    fprintf(f,"REGISTER_END result=0x%08x\n",(unsigned)rc);
    if (rc) {status=2;goto done;}
    fprintf(f,"AUDIO_LOAD_BEGIN\n");
    rc=sceSysmoduleLoadModuleInternal(AUDIOIN_ID);
    fprintf(f,"AUDIO_LOAD_END rc=0x%08x\n",(unsigned)rc);
    if(rc) {status=8;goto done;}
    audio_owned=1;
    void *create_ptr=resolve("libSceAudioIn.sprx","sceAudioInVmicCreate");
    void *destroy_ptr=resolve("libSceAudioIn.sprx","sceAudioInVmicDestroy");
    void *write_ptr=resolve("libSceAudioIn.sprx","sceAudioInVmicWrite");
    if(!create_ptr || !destroy_ptr || !write_ptr) {status=9;goto done;}
    fprintf(f,"VMIC_READY pid=%d create=%p destroy=%p write=%p\n",pid,create_ptr,destroy_ptr,write_ptr);
    for(waited=0;waited<60;waited++) {
        if(permit("/data/bluehen/service-v6.vmic-allow",pid)) break;
        sleep(1);
    }
    if(waited==60) {status=10;goto done;}
    int32_t (*create)(int32_t,int32_t,int32_t,int32_t) = (void *)create_ptr;
    int32_t (*destroy)(int32_t) = (void *)destroy_ptr;
    fprintf(f, "CREATE_BEGIN args=0,128,16000,1\n");
    int32_t created = create(0,128,16000,1);
    fprintf(f, "CREATE_END result=0x%08x\n", (unsigned)created);
    unsigned handle = 0; int cleanup = 0;
    for (unsigned i = 0; i < 45; i++) {
        FILE *control = fopen("/data/bluehen/service-v6.cleanup", "r");
        if (control) {
            int owner = -1; unsigned candidate = 0;
            int n = fscanf(control, "%d %u", &owner, &candidate); fclose(control);
            if (n == 2 && valid_cleanup(pid, owner, candidate, created)) {
                handle = candidate; cleanup = 1; break;
            }
            /* Explicit no-resource observation for an API failure. */
            if (n == 2 && owner == pid && candidate == 0 && created < 0) {
                fprintf(f, "SUPERVISOR_NO_OWNED_SLOT\n"); status = 3; goto done;
            }
        }
        sleep(1);
    }
    if (!cleanup && created >= 0 && valid_cleanup(pid,pid,(unsigned)created,created)) {
        handle = (unsigned)created; cleanup = 1;
        fprintf(f, "CLEANUP returned_handle_fallback\n");
    }
    if (!cleanup) {
        fprintf(f, "CLEANUP_UNCONFIRMED no verified handle; no guess or retry\n");
        status = 7; goto done;
    }
    int32_t write_rc=-1;
    if(created==(int32_t)handle) {
        /* Exact 9.60 ABI: handle, frame count, signed 16-bit interleaved PCM.
         * One 128-frame mono block: 256 bytes of synthetic silence. */
        const int16_t silence[128]={0};
        int32_t (*write_pcm)(int32_t,int32_t,const void *)=(void *)write_ptr;
        fprintf(f,"WRITE_BEGIN handle=0x%08x frames=128 bytes=256 silence=1\n",handle);
        write_rc=write_pcm((int32_t)handle,128,silence);
        fprintf(f,"WRITE_END result=0x%08x\n",(unsigned)write_rc);
    } else fprintf(f,"WRITE_SKIPPED create failed\n");
    fprintf(f, "DESTROY_BEGIN handle=0x%08x\n",handle);
    rc = destroy((int32_t)handle);
    fprintf(f, "DESTROY_END result=0x%08x\n",(unsigned)rc);
    status = created == (int32_t)handle && write_rc == 0 && rc == 0 ? 0 : 4;
    /* Leave observation time before normal process termination. */
    sleep(3);
 done:
    if(audio_owned) {
        fprintf(f,"AUDIO_UNLOAD_BEGIN\n");
        rc=sceSysmoduleUnloadModuleInternal(AUDIOIN_ID);
        fprintf(f,"AUDIO_UNLOAD_END rc=0x%08x\n",(unsigned)rc);
        if(rc) status=11;
    }
    if (system_owned) {
        fprintf(f,"SYSTEM_UNLOAD_BEGIN\n");
        rc=sceSysmoduleUnloadModuleInternal(0x80000010u);
        fprintf(f,"SYSTEM_UNLOAD_END rc=0x%08x\n",(unsigned)rc);
        if (rc) status=7;
    }
    fprintf(f,"END result=%d SYNTHETIC_SILENCE_ONLY NO_ROUTING NO_BLUETOOTH NO_CREDENTIAL_CHANGES\n",status);
    fclose(f);return status;
}
