/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Load/start and unload system audio modules in this process only.
 * Module entrypoints run; no AudioIn, Vmic, routing or Bluetooth API is called. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef BLUEHEN_LOG
#define BLUEHEN_LOG "/data/bluehen/module-probe-v1.log"
#define BLUEHEN_GATE "/data/bluehen/module-probe-v1.allow"
#endif

struct sw_version { size_t size; char text[28]; uint32_t version; };
_Static_assert(sizeof(struct sw_version) == 40, "firmware ABI");
int32_t sceKernelGetSystemSwVersion(struct sw_version *);
int32_t sceKernelLoadStartModule(const char *, size_t, const void *, uint32_t, void *, int32_t *);
int32_t sceKernelStopUnloadModule(int32_t, size_t, const void *, uint32_t, void *, int32_t *);
int32_t sceKernelDlsym(int32_t, const char *, void **);

static const char *modules[] = {
    "/system/common/lib/libSceIpmi.sprx",
    "/system/common/lib/libSceAudioOut.sprx",
    "/system/common/lib/libSceMbus.sprx",
    "/system/common/lib/libSceAudioIn.sprx"
};
static const char *symbols[] = {
    "sceAudioInVmicCreate", "sceAudioInVmicWrite", "sceAudioInVmicDestroy"
};

int bluehen_modules(FILE *log)
{
    struct sw_version version = { .size = sizeof(version) };
    int32_t handles[4];
    size_t count = 0;
    int result = 0;
    int32_t rc = sceKernelGetSystemSwVersion(&version);
    fprintf(log, "BLUEHEN_MODULE_PROBE_V1 pid=%d\n", (int)getpid());
    fprintf(log, "firmware rc=0x%08x code=0x%08x\n", (unsigned)rc, version.version);
    if (rc != 0 || (version.version & 0xffff0000u) != 0x09600000u)
        return 2;
    for (size_t i = 0; i < 4; i++) {
        int32_t start_result = 0;
        fprintf(log, "LOAD_BEGIN index=%zu path=%s\n", i, modules[i]);
        if (fflush(log) || ferror(log)) { result = 5; break; }
        rc = sceKernelLoadStartModule(modules[i], 0, NULL, 0, NULL, &start_result);
        fprintf(log, "LOAD_END index=%zu handle=0x%08x entry_result=0x%08x\n",
                i, (unsigned)rc, (unsigned)start_result);
        if (rc < 0) { result = 3; break; }
        handles[count++] = rc;
        if (start_result != 0) { result = 3; break; }
    }
    if (count == 4 && result == 0) {
        for (size_t i = 0; i < 3; i++) {
            void *address = NULL;
            rc = sceKernelDlsym(handles[3], symbols[i], &address);
            fprintf(log, "LOOKUP name=%s rc=0x%08x resolved=%s\n",
                    symbols[i], (unsigned)rc, rc == 0 && address ? "yes" : "no");
            if (rc || !address) result = 4;
        }
    }
    /* Only release handles acquired here, in reverse dependency order.
     * On unload failure stop: dependencies might still be in use. */
    while (count) {
        int32_t stop_result = 0;
        int32_t handle = handles[--count];
        fprintf(log, "UNLOAD_BEGIN handle=0x%08x\n", (unsigned)handle);
        fflush(log);
        rc = sceKernelStopUnloadModule(handle, 0, NULL, 0, NULL, &stop_result);
        fprintf(log, "UNLOAD_END rc=0x%08x entry_result=0x%08x\n",
                (unsigned)rc, (unsigned)stop_result);
        if (rc < 0 || stop_result != 0) {
            fprintf(log, "CLEANUP_UNCONFIRMED remaining=%zu; process exit required\n", count);
            result = 6;
            break;
        }
    }
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
