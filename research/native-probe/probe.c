/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Inventory only: no Bluetooth/AudioIn calls, module loads or device opens. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* ABI declarations from OpenOrbis include/orbis/_types/kernel.h. */
struct segment { void *address; uint32_t size; int32_t prot; };
struct module_info {
    size_t size;
    char name[256];
    struct segment segments[4];
    uint32_t segment_count;
    uint8_t fingerprint[20];
};
struct sw_version { size_t size; char text[28]; uint32_t version; };
_Static_assert(sizeof(struct module_info) == 352, "module ABI");
_Static_assert(sizeof(struct sw_version) == 40, "version ABI");
int32_t sceKernelGetSystemSwVersion(struct sw_version *);
int32_t sceKernelGetModuleList(uint32_t *, size_t, size_t *);
int32_t sceKernelGetModuleInfo(uint32_t, struct module_info *);
int32_t sceKernelDlsym(int32_t, const char *, void **);

static const char *symbols[] = {
    "sceBluetoothHidGetDeviceInfo", "scePadGetBluetoothAddress",
    "sceAudioInInit", "sceAudioInOpen", "sceAudioInInput",
    "sceAudioInClose", "sceAudioInSetConnections",
    "sceAudioOutOpen", "sceAudioOutOutput"
};

/* Returns 2 outside 9.60 and 3 if the module inventory fails. A return of 0
 * means the inventory completed, never that a Bluetooth transport works. */
int bluehen_inventory(FILE *log)
{
    struct sw_version version = { .size = sizeof(version) };
    uint32_t handles[256];
    size_t count = 0, i, j;
    int32_t rc = sceKernelGetSystemSwVersion(&version);
    fprintf(log, "BLUEHEN_NATIVE_INVENTORY_V1\n");
    fprintf(log, "firmware rc=0x%08x code=0x%08x text=%.28s\n",
            (unsigned)rc, version.version, version.text);
    if (rc != 0 || (version.version & 0xffff0000u) != 0x09600000u) {
        fprintf(log, "REFUSED: expected firmware 9.60; no inventory attempted\n");
        return 2;
    }
    /* OpenOrbis specifies size as the handle count, not byte capacity. */
    rc = sceKernelGetModuleList(handles, 256, &count);
    fprintf(log, "module_list rc=0x%08x reported=%zu capacity=256\n",
            (unsigned)rc, count);
    if (rc != 0) return 3;
    if (count > 256) {
        fprintf(log, "TRUNCATED: inspecting first 256 handles only\n");
        count = 256;
    }
    for (i = 0; i < count; i++) {
        struct module_info info = { .size = sizeof(info) };
        rc = sceKernelGetModuleInfo(handles[i], &info);
        fprintf(log, "module handle=%u rc=0x%08x name=%.256s\n",
                handles[i], (unsigned)rc, rc == 0 ? info.name : "<unknown>");
        /* Resolve by name only. None of these function pointers is called. */
        for (j = 0; j < sizeof(symbols) / sizeof(symbols[0]); j++) {
            void *address = NULL;
            rc = sceKernelDlsym((int32_t)handles[i], symbols[j], &address);
            if (rc == 0 && address != NULL)
                fprintf(log, "export handle=%u name=%s resolved=yes\n",
                        handles[i], symbols[j]);
        }
    }
    fprintf(log, "MODULE_INVENTORY_COMPLETE scope=current_process_only\n");
    fprintf(log, "Unresolved exports may exist in other processes/unloaded modules.\n");
    fprintf(log, "A2DP_transport=UNTESTED HFP_transport=UNTESTED mic_routing=UNTESTED\n");
    return 0;
}

#ifndef BLUEHEN_HOST_TEST
static void list_candidates(FILE *log, const char *directory)
{
    DIR *dir = opendir(directory);
    struct dirent *entry;
    unsigned inspected = 0;
    if (!dir) {
        fprintf(log, "directory=%s errno=%d\n", directory, errno);
        return;
    }
    while (inspected < 4096 && (entry = readdir(dir)) != NULL) {
        inspected++;
        if (strstr(entry->d_name, "bluetooth") ||
            strstr(entry->d_name, "Bluetooth") ||
            strstr(entry->d_name, "audio") ||
            strstr(entry->d_name, "Audio") ||
            strstr(entry->d_name, "hid"))
            fprintf(log, "candidate=%s/%s (name only, not opened)\n",
                    directory, entry->d_name);
    }
    if (inspected == 4096) fprintf(log, "directory=%s scan_limit=4096\n", directory);
    closedir(dir);
}

int main(void)
{
    const char *paths[] = { "/data/bluehen", "/user/data/bluehen" };
    FILE *log = NULL;
    unsigned i;
    char path[96];
    int result;
    for (i = 0; i < 2 && !log; i++) {
        if (mkdir(paths[i], 0700) != 0 && errno != EEXIST) continue;
        snprintf(path, sizeof(path), "%s/native-probe.log", paths[i]);
        log = fopen(path, "a"); /* Preserve earlier observations. */
    }
    if (!log) {
        puts("BlueHEN: cannot open inventory log; stopped");
        return 1;
    }
    result = bluehen_inventory(log);
    if (result == 0) {
        list_candidates(log, "/dev");
        list_candidates(log, "/system/common/lib");
        list_candidates(log, "/system/priv/lib");
    }
    fprintf(log, "END result=%d\n", result);
    if (fflush(log) != 0 || ferror(log)) result = 4;
    if (fclose(log) != 0) result = 4;
    printf("BlueHEN native inventory result=%d log=%s\n", result, path);
    return result;
}
#endif
