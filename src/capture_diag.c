/* SPDX-License-Identifier: GPL-3.0-or-later */
/* capture_diag - probe PS4 system-audio capture points for HearBridge.
 *
 * Logs what is loadable from a GoldHEN payload context:
 *   - libSceAudioOut symbols (sceAudioOutGetSystemState / GetInfoOpenNum / ...)
 *   - libSceAvcap (internal sysmodule 0x80000003) load + symbol resolve
 *   - notes that home-screen UI audio is produced by SceShellUI via
 *     sceAudioOutOutput, so a GoldHEN PRX hook is the practical first capture
 *     path (see docs/capture.md).
 *
 * Does NOT inject into SceShellUI. Write results to
 * /data/hearbridge/capture_diag.log and show a short notification.
 */
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_PATH     "/data/hearbridge/capture_diag.log"
#define LOG_PATH_ALT "/user/data/hearbridge/capture_diag.log"

int32_t sceKernelLoadStartModule(const char *path, size_t argc, const void *argv,
                                  uint32_t flags, void *opt, int *res);
int32_t sceKernelDlsym(int32_t handle, const char *sym, void **addr);
int32_t sceKernelSendNotificationRequest(int32_t, void *, size_t, int32_t);

struct notify_req {
    int32_t type, req_id, priority, msg_id, target_id, user_id, unk1, unk2, app_id, error_num, unk3;
    char use_icon_uri;
    char message[1024];
    char icon_uri[1024];
    char unk[1024];
};

static FILE *g_log;

static void out(const char *fmt, ...)
{
    va_list ap;
    char line[512];
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    puts(line);
    if (g_log) {
        fputs(line, g_log);
        fputc('\n', g_log);
        fflush(g_log);
    }
}

static void notify(const char *fmt, ...)
{
    static struct notify_req r;
    va_list ap;
    memset(&r, 0, sizeof r);
    r.type = 0;
    r.use_icon_uri = 1;
    snprintf(r.icon_uri, sizeof r.icon_uri, "cxml://psnotification/tex_icon_system");
    va_start(ap, fmt);
    vsnprintf(r.message, sizeof r.message, fmt, ap);
    va_end(ap);
    sceKernelSendNotificationRequest(0, &r, sizeof r, 0);
}

static int32_t try_load(const char *path)
{
    int res = 0;
    int32_t h = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, &res);
    out("LoadStart %s -> handle 0x%08x res %d", path, (unsigned)h, res);
    return h;
}

static void try_sym(int32_t h, const char *name)
{
    void *p = NULL;
    int32_t rc;
    if (h < 0) {
        out("  skip %s (no module)", name);
        return;
    }
    rc = sceKernelDlsym(h, name, &p);
    out("  dlsym %s -> rc 0x%08x addr %p", name, (unsigned)rc, p);
}

int main(void)
{
    int32_t ha = -1, hv = -1, hs = -1;
    typedef int32_t (*fn_init)(void);
    typedef int32_t (*fn_sys)(void *);
    typedef int32_t (*fn_open_num)(void);
    fn_init Init = NULL;
    fn_sys GetSys = NULL;
    fn_open_num OpenNum = NULL;
    unsigned char sysbuf[64];
    int32_t rc;

    mkdir("/data/hearbridge", 0777);
    mkdir("/user/data/hearbridge", 0777);
    g_log = fopen(LOG_PATH, "w");
    if (!g_log) g_log = fopen(LOG_PATH_ALT, "w");

    out("HearBridge capture_diag");
    notify("HearBridge capture_diag");

    out("--- libSceAudioOut ---");
    ha = try_load("/system/common/lib/libSceAudioOut.sprx");
    if (ha < 0) ha = try_load("libSceAudioOut.sprx");
    try_sym(ha, "sceAudioOutInit");
    try_sym(ha, "sceAudioOutOpen");
    try_sym(ha, "sceAudioOutOutput");
    try_sym(ha, "sceAudioOutOutputs");
    try_sym(ha, "sceAudioOutGetPortState");
    try_sym(ha, "sceAudioOutGetSystemState");
    try_sym(ha, "sceAudioOutGetInfoOpenNum");
    try_sym(ha, "sceAudioOutGetInfo");
    try_sym(ha, "sceAudioOutGetHandleStatusInfo");

    if (ha >= 0) {
        sceKernelDlsym(ha, "sceAudioOutInit", (void **)&Init);
        sceKernelDlsym(ha, "sceAudioOutGetSystemState", (void **)&GetSys);
        sceKernelDlsym(ha, "sceAudioOutGetInfoOpenNum", (void **)&OpenNum);
        if (Init) {
            rc = Init();
            out("sceAudioOutInit -> 0x%08x", (unsigned)rc);
        }
        if (GetSys) {
            memset(sysbuf, 0, sizeof sysbuf);
            rc = GetSys(sysbuf);
            out("sceAudioOutGetSystemState -> 0x%08x", (unsigned)rc);
            out("  state bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
                sysbuf[0], sysbuf[1], sysbuf[2], sysbuf[3],
                sysbuf[4], sysbuf[5], sysbuf[6], sysbuf[7]);
        }
        if (OpenNum) {
            rc = OpenNum();
            out("sceAudioOutGetInfoOpenNum -> %d (0x%08x)", (int)rc, (unsigned)rc);
            out("NOTE: open-num in THIS process only counts ports opened here,");
            out("not SceShellUI home-screen ports. Those live in another process.");
        }
    }

    out("--- libSceAvcap (internal 0x80000003) ---");
    hv = try_load("/system/common/lib/libSceAvcap.sprx");
    if (hv < 0) hv = try_load("/system/priv/lib/libSceAvcap.sprx");
    if (hv < 0) hv = try_load("libSceAvcap.sprx");
    try_sym(hv, "sceAvcapInitialize");
    try_sym(hv, "sceAvcapTerminate");
    try_sym(hv, "sceAvcapOpen");
    try_sym(hv, "sceAvcapOpenAudio");
    try_sym(hv, "sceAvcapClose");
    try_sym(hv, "sceAvcapStart");
    try_sym(hv, "sceAvcapStop");
    try_sym(hv, "sceAvcapRead");
    try_sym(hv, "sceAvcapReadAudio");
    if (hv < 0) {
        out("Avcap not loadable from this payload context (expected without");
        out("sceSysmoduleLoadModuleInternal / elevated authid). ABI is also");
        out("undocumented for PS4 homebrew, unlike PS5 libSceAvcap2.");
    }

    out("--- libSceSysmodule ---");
    hs = try_load("/system/common/lib/libSceSysmodule.sprx");
    if (hs < 0) hs = try_load("libSceSysmodule.sprx");
    try_sym(hs, "sceSysmoduleLoadModule");
    try_sym(hs, "sceSysmoduleLoadModuleInternal");
    try_sym(hs, "sceSysmoduleIsLoaded");

    out("--- conclusion ---");
    out("Home screen / SceShellUI sounds: capture by GoldHEN PRX hooking");
    out("sceAudioOutOutput (and sceAudioOutOutputs) in SceShellUI, tee PCM");
    out("to UDP 127.0.0.1:%d for the HearBridge daemon. See docs/capture.md", 9500);
    out("and plugin/shell_audio_tap.c.");

    notify("capture_diag done (see log)");
    out("done");
    if (g_log) fclose(g_log);
    return 0;
}
