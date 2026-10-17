/* SPDX-License-Identifier: GPL-3.0-or-later */
/* hearbridge_game_min - load-test GoldHEN game plugin, built as a ladder.
 *
 * STEP 1: plugin_template behaviour only (klog + "Hello" notification).
 * STEP 2: STEP 1 + title ID from sys_sdk_proc_info (oversized buffer).
 * STEP 3: STEP 2 + one line appended to /data/GoldHEN/hearbridge_game_min.log
 *         with sceKernelOpen/Write/Close (same calls GoldHEN's plugin_loader
 *         uses to write /data/GoldHEN/plugins.ini). This is
 *         build/hearbridge_game_min.prx.
 * No hooks, no network, empty module_start / module_stop.
 */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "hb_plugin_common.h"

#ifndef STEP
#define STEP 3
#endif

#if STEP == 1
#define MIN_NAME "hearbridge_step1"
#elif STEP == 2
#define MIN_NAME "hearbridge_step2"
#else
#define MIN_NAME "hearbridge_game_min"
#endif

#define MIN_LOG "/data/GoldHEN/hearbridge_game_min.log"
#define K_O_WRONLY 0x0001
#define K_O_APPEND 0x0008
#define K_O_CREAT  0x0200

attr_public const char *g_pluginName = MIN_NAME;
attr_public const char *g_pluginDesc = "HearBridge load test";
attr_public const char *g_pluginAuth = "HearBridge";
attr_public uint32_t g_pluginVersion = 0x00000100; /* 1.0.0 */

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    char msg[128] = {0};
#if STEP >= 2
    char title[16] = {0};
    int rc;
#endif
    (void)argc;
    (void)argv;

    final_printf("[GoldHEN] %s Plugin Started.\n", g_pluginName);
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
#if STEP == 1
    snprintf(msg, sizeof(msg), "Hello from %s", g_pluginName);
#else
    rc = hb_title_id(title, sizeof title);
    final_printf("[GoldHEN] %s proc_info rc=%d title=%s\n", g_pluginName, rc, title);
    snprintf(msg, sizeof(msg), "Hello from %s\n%s rc=%d", g_pluginName,
             title[0] ? title : "unknown", rc);
#endif
#if STEP >= 3
    {
        int32_t fd = sceKernelOpen(MIN_LOG, K_O_WRONLY | K_O_CREAT | K_O_APPEND, 0777);
        final_printf("[GoldHEN] %s open %s = 0x%08x\n", g_pluginName, MIN_LOG, (unsigned)fd);
        if (fd >= 0) {
            char line[96];
            snprintf(line, sizeof line, "%s loaded title=%s rc=%d\n", g_pluginName,
                     title[0] ? title : "unknown", rc);
            sceKernelWrite(fd, line, strlen(line));
            sceKernelClose(fd);
        }
    }
#endif
    hb_notify(msg);
    return 0;
}

int32_t attr_public plugin_unload(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    return 0;
}

int32_t attr_module_hidden module_start(int64_t argc, const void *args)
{
    (void)argc;
    (void)args;
    return 0;
}

int32_t attr_module_hidden module_stop(int64_t argc, const void *args)
{
    (void)argc;
    (void)args;
    return 0;
}
