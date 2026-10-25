/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Load/start and unload system audio modules in this process only.
 * Module entrypoints run; no AudioIn, Vmic, routing or Bluetooth API is called. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#ifndef BLUEHEN_LOG
#define BLUEHEN_LOG "/data/bluehen/module-probe-v2.log"
#define BLUEHEN_GATE "/data/bluehen/module-probe-v2.allow"
#endif

struct sw_version { size_t size; char text[28]; uint32_t version; };
_Static_assert(sizeof(struct sw_version) == 40, "firmware ABI");
int32_t sceKernelGetSystemSwVersion(struct sw_version *);
/* ABI declarations from OpenOrbis kernel and Sysmodule headers. */
int32_t sceSysmoduleLoadModuleInternal(uint32_t);
int32_t sceSysmoduleUnloadModuleInternal(uint32_t);
int32_t sceKernelDlsym(int32_t, const char *, void **);
struct segment { void *address; uint32_t size; int32_t prot; };
struct module_info {
    size_t size; char name[256]; struct segment segments[4];
    uint32_t segment_count; uint8_t fingerprint[20];
};
_Static_assert(sizeof(struct module_info) == 352, "module ABI");
int32_t sceKernelGetModuleList(uint32_t *, size_t, size_t *);
int32_t sceKernelGetModuleInfo(uint32_t, struct module_info *);
#define AUDIOIN_ID 0x80000002u

/* Return handle, -1 for absent, -2 for failed/ambiguous inventory. */
static int32_t audioin_handle(FILE *log)
{
    uint32_t handles[256]; size_t count = 0; int32_t found = -1;
    if (sceKernelGetModuleList(handles, 256, &count) || count > 256) return -2;
    for (size_t i = 0; i < count; i++) {
        struct module_info info = { .size = sizeof(info) };
        if (sceKernelGetModuleInfo(handles[i], &info)) return -2;
        if (!memchr(info.name, 0, sizeof(info.name))) return -2;
        fprintf(log, "MODULE handle=%u name=%s\n", handles[i], info.name);
        if (!strcmp(info.name, "libSceAudioIn.sprx") || !strcmp(info.name, "libSceAudioIn")) {
            if (found >= 0) return -2;
            found = (int32_t)handles[i];
        }
    }
    return found;
}
static const char *symbols[] = {
    "sceAudioInVmicCreate", "sceAudioInVmicWrite", "sceAudioInVmicDestroy"
};

int bluehen_modules(FILE *log)
{
    struct sw_version version = { .size = sizeof(version) };
    int32_t rc = sceKernelGetSystemSwVersion(&version);
    int result = 0;
    fprintf(log, "BLUEHEN_MODULE_PROBE_V2 pid=%d\n", (int)getpid());
    fprintf(log, "firmware rc=0x%08x code=0x%08x\n", (unsigned)rc, version.version);
    if (rc || (version.version & 0xffff0000u) != 0x09600000u) return 2;
    if (audioin_handle(log) != -1) {
        fprintf(log, "REFUSED AudioIn already present or inventory uncertain\n");
        return 8;
    }
    fprintf(log, "SYSMODULE_LOAD_BEGIN id=0x%08x\n", AUDIOIN_ID);
    if (fflush(log) || ferror(log)) return 5;
    rc = sceSysmoduleLoadModuleInternal(AUDIOIN_ID);
    fprintf(log, "SYSMODULE_LOAD_END rc=0x%08x\n", (unsigned)rc);
    if (rc) return 3;
    /* The module manager owns dependency loading. Release exactly our one
     * successful acquisition with the same API, never raw kernel unload. */
    int32_t handle = audioin_handle(log);
    if (handle < 0) result = 4;
    else for (size_t i = 0; i < 3; i++) {
        void *address = NULL;
        rc = sceKernelDlsym(handle, symbols[i], &address);
        fprintf(log, "LOOKUP name=%s rc=0x%08x resolved=%s\n",
                symbols[i], (unsigned)rc, !rc && address ? "yes" : "no");
        if (rc || !address) result = 4;
    }
    fprintf(log, "SYSMODULE_UNLOAD_BEGIN id=0x%08x\n", AUDIOIN_ID);
    fflush(log);
    rc = sceSysmoduleUnloadModuleInternal(AUDIOIN_ID);
    fprintf(log, "SYSMODULE_UNLOAD_END rc=0x%08x\n", (unsigned)rc);
    if (rc) result = 6;
    else if (audioin_handle(log) != -1) result = 9;
    fprintf(log, "NO_VMIC_CALLS NO_ROUTING_CALLS NO_BLUETOOTH_CALLS\n");
    return result;
}

#ifndef BLUEHEN_MODULE_HOST_TEST
int main(void)
{
    const char *path = BLUEHEN_LOG;
    if (mkdir("/data/bluehen", 0700) && errno != EEXIST) return 1;
    FILE *log = fopen(path, "a");
    if (!log) return 1;
    /* Host verifies that this is a new process before granting this run's PID.
     * Never load audio modules into a pre-existing system process. */
    int self = (int)getpid(), allowed = 0;
    fprintf(log, "READY pid=%d waiting_for_process_identity_check\n", self);
    if (fflush(log)) { fclose(log); return 5; }
    for (int i = 0; i < 60 && !allowed; i++) {
        FILE *gate = fopen(BLUEHEN_GATE, "r");
        if (gate) {
            int permit = -1;
            if (fscanf(gate, "%d", &permit) == 1 && permit == self) allowed = 1;
            fclose(gate);
        }
        if (!allowed) sleep(1);
    }
    if (!allowed) {
        fprintf(log, "REFUSED process identity not confirmed\nEND result=7\n");
        fclose(log);
        return 7;
    }
    int result = bluehen_modules(log);
    fprintf(log, "END result=%d\n", result);
    if (fflush(log) || ferror(log)) result = 5;
    if (fclose(log)) result = 5;
    return result;
}
#endif
