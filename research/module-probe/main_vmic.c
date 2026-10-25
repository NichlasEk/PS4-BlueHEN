/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Firmware-specific experiment, ordinary application main thread only. */
#define BLUEHEN_MODULE_HOST_TEST
#include "probe.c"

static int valid_cleanup(int self, int permit_pid, unsigned handle, int32_t created)
{
    return self == permit_pid && (handle & 0xffff00fcu) == 0x40000u &&
           (created < 0 || handle == (unsigned)created);
}

#ifndef BLUEHEN_MAIN_HOST_TEST
static int wait_pid(const char *path, int self)
{
    for (unsigned i = 0; i < 60; i++) {
        FILE *f = fopen(path, "r");
        if (f) { int pid = -1; int n = fscanf(f, "%d", &pid); fclose(f);
            if (n == 1 && pid == self) return 1; }
        sleep(1);
    }
    return 0;
}

int main(void)
{
    int self = (int)getpid(), status = 1;
    if (mkdir("/data/bluehen", 0777) && errno != EEXIST) return 1;
    FILE *log = fopen("/data/bluehen/vmic-main-v3.log", "a");
    if (!log) return 1;
    setvbuf(log, NULL, _IONBF, 0);
    struct sw_version version = { .size = sizeof(version) };
    int32_t rc = sceKernelGetSystemSwVersion(&version);
    fprintf(log, "VMIC_MAIN_V3 pid=%d firmware_rc=0x%08x firmware=0x%08x\n",
            self, (unsigned)rc, version.version);
    if (rc || (version.version & 0xffff0000u) != 0x09600000u) goto done;
    int32_t module = audioin_handle(log);
    if (module < 0) { fprintf(log, "REFUSED AudioIn not preloaded\n"); goto done; }
    void *create_ptr = NULL, *destroy_ptr = NULL;
    if (sceKernelDlsym(module, "sceAudioInVmicCreate", &create_ptr) || !create_ptr ||
        sceKernelDlsym(module, "sceAudioInVmicDestroy", &destroy_ptr) || !destroy_ptr) goto done;
    fprintf(log, "READY pid=%d create=%p destroy=%p borrowed_module=%d\n",
            self, create_ptr, destroy_ptr, module);
    if (!wait_pid("/data/bluehen/vmic-main-v3.allow", self)) {
        fprintf(log, "REFUSED supervisor preflight timeout\n"); goto done;
    }
    int32_t (*create)(int32_t,int32_t,int32_t,int32_t) = (void *)create_ptr;
    int32_t (*destroy)(int32_t) = (void *)destroy_ptr;
    fprintf(log, "CREATE_BEGIN args=0,128,16000,1\n");
    int32_t created = create(0,128,16000,1);
    fprintf(log, "CREATE_END result=0x%08x\n", (unsigned)created);
    unsigned handle = 0; int cleanup = 0;
    for (unsigned i = 0; i < 45; i++) {
        FILE *f = fopen("/data/bluehen/vmic-main-v3.cleanup", "r");
        if (f) {
            int owner = -1; unsigned candidate = 0;
            int n = fscanf(f, "%d %u", &owner, &candidate); fclose(f);
            if (n == 2 && valid_cleanup(self, owner, candidate, created)) {
                handle = candidate; cleanup = 1; break;
            }
            /* Explicit no-resource observation for an API failure. */
            if (n == 2 && owner == self && candidate == 0 && created < 0) {
                fprintf(log, "SUPERVISOR_NO_OWNED_SLOT\n"); status = 3; goto done;
            }
        }
        sleep(1);
    }
    if (!cleanup && created >= 0 && valid_cleanup(self,self,(unsigned)created,created)) {
        handle = (unsigned)created; cleanup = 1;
        fprintf(log, "CLEANUP returned_handle_fallback\n");
    }
    if (!cleanup) {
        fprintf(log, "CLEANUP_UNCONFIRMED no verified handle; no guess or retry\n");
        status = 7; goto done;
    }
    fprintf(log, "DESTROY_BEGIN handle=0x%08x\n",handle);
    rc = destroy((int32_t)handle);
    fprintf(log, "DESTROY_END result=0x%08x\n",(unsigned)rc);
    status = created == (int32_t)handle && rc == 0 ? 0 : 4;
    /* Leave observation time before normal process termination. */
    sleep(3);
 done:
    fprintf(log, "END result=%d NO_WRITE NO_ROUTING NO_BLUETOOTH\n",status);
    fclose(log);
    return status;
}
#endif
