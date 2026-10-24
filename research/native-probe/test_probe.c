/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include "probe.c"
static uint32_t firmware = 0x09600001;
static int firmware_result, list_result, list_calls, info_calls, symbol_calls;
static size_t reported = 1;
int32_t sceKernelGetSystemSwVersion(struct sw_version *v)
{
    assert(v->size == sizeof(*v));
    v->version = firmware;
    memset(v->text, 'X', sizeof(v->text)); /* Deliberately not NUL terminated. */
    return firmware_result;
}
int32_t sceKernelGetModuleList(uint32_t *h, size_t capacity, size_t *n)
{
    list_calls++;
    assert(capacity == 256);
    for (size_t i = 0; i < capacity; i++) h[i] = (uint32_t)i;
    *n = reported;
    return list_result;
}
int32_t sceKernelGetModuleInfo(uint32_t h, struct module_info *v)
{
    (void)h;
    assert(v->size == sizeof(*v));
    info_calls++;
    memset(v->name, 'M', sizeof(v->name));
    return 0;
}
int32_t sceKernelDlsym(int32_t h, const char *name, void **p)
{
    (void)h;
    symbol_calls++;
    if (strcmp(name, "sceAudioInInput") != 0) return -1;
    *p = (void *)(uintptr_t)1; /* Cannot safely be called; lookup only. */
    return 0;
}
static void reset(void) { list_calls = info_calls = symbol_calls = 0; }
int main(void)
{
    FILE *log = tmpfile();
    assert(log);
    firmware = 0x09000001;
    assert(bluehen_inventory(log) == 2 && list_calls == 0);
    firmware = 0x09600001; firmware_result = -1;
    assert(bluehen_inventory(log) == 2 && list_calls == 0);
    firmware_result = 0; list_result = -1;
    assert(bluehen_inventory(log) == 3 && info_calls == 0);
    list_result = 0; reset();
    assert(bluehen_inventory(log) == 0 && info_calls == 1 && symbol_calls == 9);
    reported = 300; reset();
    assert(bluehen_inventory(log) == 0 && info_calls == 256 && symbol_calls == 2304);
    reported = 0; reset();
    assert(bluehen_inventory(log) == 0 && info_calls == 0 && symbol_calls == 0);
    assert(fclose(log) == 0);
    puts("native probe: firmware gates, failures, bounds and lookup-only tests PASS");
    return 0;
}
