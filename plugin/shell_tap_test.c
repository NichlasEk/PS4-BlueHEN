/* SPDX-License-Identifier: GPL-3.0-or-later */
/* shell_tap_test - minimal GoldHEN PRX: prove PROCESS_PRX_LOAD into SceShellUI
 * is safe. No audio hooks. module_start only writes tap.log + tap.loaded.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <Common.h>

#define final_printf(fmt, ...) printf((fmt), ##__VA_ARGS__)
#define attr_public __attribute__((visibility("default")))
#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define s32 int32_t
#define s64 int64_t

#define PLUGIN_VER 0x00000100u
#define HB_DIR     "/data/hearbridge"
#define HB_DIR_ALT "/user/data/hearbridge"
#define TAP_LOG    HB_DIR "/tap.log"
#define TAP_LOG_A  HB_DIR_ALT "/tap.log"
#define TAP_MARK   HB_DIR "/tap.loaded"
#define TAP_MARK_A HB_DIR_ALT "/tap.loaded"
/* ShellUI's jail has no /data; fall back to dirs inside its sandbox
 * (on disk: /mnt/sandbox/NPXS20001_000/system_tmp or /tmp). */
#define TAP_LOG_S  "/system_tmp/hearbridge_tap.log"
#define TAP_LOG_T  "/tmp/hearbridge_tap.log"

attr_public const char *g_pluginName = "shell_tap_test";
attr_public const char *g_pluginDesc = "Minimal ShellUI load test (no audio hooks)";
attr_public const char *g_pluginAuth = "HearBridge";
attr_public uint32_t g_pluginVersion = PLUGIN_VER;

static void write_marker(void)
{
    int fd;
    char buf[128];
    int n;
    int pid = (int)getpid();

    mkdir(HB_DIR, 0777);
    mkdir(HB_DIR_ALT, 0777);

    n = snprintf(buf, sizeof buf, "loaded pid=%d plugin=%s\n", pid, g_pluginName);

    fd = open(TAP_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        fd = open(TAP_LOG_A, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        fd = open(TAP_LOG_S, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        fd = open(TAP_LOG_T, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        (void)write(fd, buf, (size_t)n);
        fsync(fd);
        close(fd);
    }

    n = snprintf(buf, sizeof buf, "%s\npid=%d\n", g_pluginName, pid);
    fd = open(TAP_MARK, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        fd = open(TAP_MARK_A, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        (void)write(fd, buf, (size_t)n);
        fsync(fd);
        close(fd);
    }
}

static int do_load(void)
{
    final_printf("[shell_tap_test] start pid=%d\n", (int)getpid());
    write_marker();
    final_printf("[shell_tap_test] wrote tap.log / tap.loaded\n");
    return 0;
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    return do_load();
}

int32_t attr_public plugin_unload(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    unlink(TAP_MARK);
    unlink(TAP_MARK_A);
    return 0;
}

/* Direct LoadStartModule / sys_sdk_proc_prx_load path. */
s32 attr_module_hidden module_start(s64 argc, const void *args)
{
    (void)argc;
    (void)args;
    return do_load();
}

s32 attr_module_hidden module_stop(s64 argc, const void *args)
{
    (void)argc;
    (void)args;
    unlink(TAP_MARK);
    unlink(TAP_MARK_A);
    return 0;
}
