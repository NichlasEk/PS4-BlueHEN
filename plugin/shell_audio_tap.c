/* SPDX-License-Identifier: GPL-3.0-or-later */
/* shell_audio_tap - GoldHEN PRX: tee SceShellUI AudioOut PCM to HearBridge.
 *
 * Safety rules:
 *  - module_start / plugin_load only install hooks (no notify, no net, no I/O
 *    beyond a tiny load marker).
 *  - UDP socket created lazily once on first tee, non-blocking sendto.
 *  - Never block or allocate on the audio thread; drop on EAGAIN.
 *  - tap.log written once on first hook hit.
 *  - Unknown formats are skipped (not assumed).
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <Common.h>
#include <orbis/AudioOut.h>
#include <orbis/Net.h>
#include <orbis/_types/net.h>
#include <orbis/_types/audio_out.h>

#define final_printf(fmt, ...) printf((fmt), ##__VA_ARGS__)
#define attr_public __attribute__((visibility("default")))
#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define s32 int32_t
#define s64 int64_t

#define PLUGIN_VER 0x00000100u
#define PCM_PORT   9500
#define MAX_PORTS  32
#define MAX_FRAMES 1024
#define MSG_DONTWAIT_ORBIS 0x80

#define HB_DIR     "/data/hearbridge"
#define HB_DIR_ALT "/user/data/hearbridge"
#define TAP_LOG    HB_DIR "/tap.log"
#define TAP_LOG_A  HB_DIR_ALT "/tap.log"
#define TAP_MARK   HB_DIR "/tap.loaded"
#define TAP_MARK_A HB_DIR_ALT "/tap.loaded"

attr_public const char *g_pluginName = "shell_audio_tap";
attr_public const char *g_pluginDesc = "Tee SceShellUI AudioOut to HearBridge UDP :9500";
attr_public const char *g_pluginAuth = "HearBridge";
attr_public uint32_t g_pluginVersion = PLUGIN_VER;

struct port_info {
    int32_t handle;
    uint32_t frames;
    uint32_t rate;
    uint32_t format;
    int in_use;
};

struct hb_sockaddr_in {
    uint8_t sin_len;
    uint8_t sin_family;
    uint16_t sin_port;
    OrbisNetInAddr sin_addr;
    char sin_zero[8];
};

#ifndef SCE_NET_SOL_SOCKET
#define SCE_NET_SOL_SOCKET 0xffff
#endif
#ifndef SCE_NET_SO_NBIO
#define SCE_NET_SO_NBIO 0x1200
#endif

static struct port_info g_ports[MAX_PORTS];
static OrbisNetId g_sock = -1;
static struct hb_sockaddr_in g_dst;
static volatile int g_net_state; /* 0 none, 1 initing, 2 ok, -1 fail */
static int g_active;
static int g_first_hit_logged;
static int16_t g_conv[MAX_FRAMES * 2];

HOOK_INIT(sceAudioOutOpen);
HOOK_INIT(sceAudioOutClose);
HOOK_INIT(sceAudioOutOutput);
HOOK_INIT(sceAudioOutOutputs);

static void write_loaded_marker(void)
{
    int fd;
    char buf[128];
    int n;
    int pid = (int)getpid();

    mkdir(HB_DIR, 0777);
    mkdir(HB_DIR_ALT, 0777);
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

static void log_first_hit(const char *which, int32_t handle)
{
    int fd;
    char buf[192];
    int n;

    if (g_first_hit_logged)
        return;
    g_first_hit_logged = 1;

    mkdir(HB_DIR, 0777);
    mkdir(HB_DIR_ALT, 0777);
    n = snprintf(buf, sizeof buf,
                 "hook_hit fn=%s handle=%d pid=%d plugin=%s ver=0x%08x\n",
                 which, (int)handle, (int)getpid(), g_pluginName, PLUGIN_VER);
    fd = open(TAP_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        fd = open(TAP_LOG_A, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        (void)write(fd, buf, (size_t)n);
        fsync(fd);
        close(fd);
    }
}

/* Lazy, once. Safe to call from the audio thread: no blocking connect,
 * non-blocking socket, sendto only. */
static void net_ensure(void)
{
    int nbio = 1;

    if (g_net_state == 2 || g_net_state == -1)
        return;
    if (g_net_state == 1)
        return;
    g_net_state = 1;

    g_sock = sceNetSocket("hb_shell_tap", ORBIS_NET_AF_INET,
                          ORBIS_NET_SOCK_DGRAM, 0);
    if (g_sock < 0) {
        g_net_state = -1;
        return;
    }
    (void)sceNetSetsockopt(g_sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO,
                           &nbio, sizeof nbio);
    memset(&g_dst, 0, sizeof g_dst);
    g_dst.sin_len = (uint8_t)sizeof g_dst;
    g_dst.sin_family = ORBIS_NET_AF_INET;
    g_dst.sin_port = __builtin_bswap16((uint16_t)PCM_PORT);
    g_dst.sin_addr.s_addr = __builtin_bswap32(0x7f000001u);
    g_net_state = 2;
}

static struct port_info *port_find(int32_t handle)
{
    int i;
    for (i = 0; i < MAX_PORTS; i++)
        if (g_ports[i].in_use && g_ports[i].handle == handle)
            return &g_ports[i];
    return NULL;
}

static struct port_info *port_alloc(int32_t handle)
{
    int i;
    struct port_info *p = port_find(handle);
    if (p)
        return p;
    for (i = 0; i < MAX_PORTS; i++) {
        if (!g_ports[i].in_use) {
            memset(&g_ports[i], 0, sizeof g_ports[i]);
            g_ports[i].in_use = 1;
            g_ports[i].handle = handle;
            return &g_ports[i];
        }
    }
    return NULL;
}

static void port_free(int32_t handle)
{
    struct port_info *p = port_find(handle);
    if (p)
        p->in_use = 0;
}

static void send_s16_stereo(const int16_t *pcm, int frames)
{
    int off = 0;
    if (!pcm || frames <= 0)
        return;
    net_ensure();
    if (g_net_state != 2 || g_sock < 0)
        return;
    while (off < frames) {
        int n = frames - off;
        int rc;
        if (n > MAX_FRAMES)
            n = MAX_FRAMES;
        rc = sceNetSendto(g_sock, pcm + off * 2, (size_t)n * 4u,
                          MSG_DONTWAIT_ORBIS,
                          (const OrbisNetSockaddr *)&g_dst, sizeof g_dst);
        if (rc < 0)
            break;
        off += n;
    }
}

static void tee_buffer(const struct port_info *pi, const void *ptr)
{
    uint32_t frames;
    uint32_t fmt;
    uint32_t i;

    if (!ptr || !pi)
        return;
    frames = pi->frames ? pi->frames : 256;
    if (frames > MAX_FRAMES)
        frames = MAX_FRAMES;
    fmt = pi->format & 0xff;

    /* Daemon expects 48 kHz; skip other rates rather than stretch on audio thread. */
    if (pi->rate && pi->rate != 48000)
        return;

    switch (fmt) {
    case ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO:
        send_s16_stereo((const int16_t *)ptr, (int)frames);
        break;
    case ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_MONO: {
        const int16_t *s = (const int16_t *)ptr;
        for (i = 0; i < frames; i++) {
            g_conv[i * 2] = s[i];
            g_conv[i * 2 + 1] = s[i];
        }
        send_s16_stereo(g_conv, (int)frames);
        break;
    }
    case ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO: {
        const float *f = (const float *)ptr;
        for (i = 0; i < frames; i++) {
            float l = f[i * 2], r = f[i * 2 + 1];
            if (l > 1.f) l = 1.f;
            if (l < -1.f) l = -1.f;
            if (r > 1.f) r = 1.f;
            if (r < -1.f) r = -1.f;
            g_conv[i * 2] = (int16_t)(l * 32767.f);
            g_conv[i * 2 + 1] = (int16_t)(r * 32767.f);
        }
        send_s16_stereo(g_conv, (int)frames);
        break;
    }
    case ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO: {
        const float *f = (const float *)ptr;
        for (i = 0; i < frames; i++) {
            float v = f[i];
            int16_t s;
            if (v > 1.f) v = 1.f;
            if (v < -1.f) v = -1.f;
            s = (int16_t)(v * 32767.f);
            g_conv[i * 2] = s;
            g_conv[i * 2 + 1] = s;
        }
        send_s16_stereo(g_conv, (int)frames);
        break;
    }
    default:
        /* Unknown layout: skip (do not guess). */
        break;
    }
}

static int32_t sceAudioOutOpen_hook(OrbisUserServiceUserId userId,
                                    enum OrbisAudioOutPort type, int32_t index,
                                    uint32_t len, uint32_t freq, uint32_t param)
{
    int32_t h;
    struct port_info *p;

    log_first_hit("sceAudioOutOpen", 0);
    h = HOOK_CONTINUE(sceAudioOutOpen,
                      int32_t (*)(OrbisUserServiceUserId, enum OrbisAudioOutPort,
                                  int32_t, uint32_t, uint32_t, uint32_t),
                      userId, type, index, len, freq, param);
    if (h > 0) {
        p = port_alloc(h);
        if (p) {
            p->frames = len;
            p->rate = freq;
            p->format = param;
        }
    }
    return h;
}

static int32_t sceAudioOutClose_hook(int32_t handle)
{
    port_free(handle);
    return HOOK_CONTINUE(sceAudioOutClose, int32_t (*)(int32_t), handle);
}

static int32_t sceAudioOutOutput_hook(int32_t handle, const void *ptr)
{
    struct port_info *p;

    log_first_hit("sceAudioOutOutput", handle);
    if (ptr) {
        p = port_find(handle);
        if (!p) {
            struct port_info tmp;
            memset(&tmp, 0, sizeof tmp);
            tmp.frames = 256;
            tmp.rate = 48000;
            tmp.format = ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO;
            tee_buffer(&tmp, ptr);
        } else {
            tee_buffer(p, ptr);
        }
    }
    return HOOK_CONTINUE(sceAudioOutOutput, int32_t (*)(int32_t, const void *),
                         handle, ptr);
}

static int32_t sceAudioOutOutputs_hook(void *param, uint32_t num)
{
    uint32_t i;
    OrbisAudioOutOutputParam *p = (OrbisAudioOutOutputParam *)param;

    log_first_hit("sceAudioOutOutputs", 0);
    if (p) {
        for (i = 0; i < num; i++) {
            if (p[i].pointer) {
                struct port_info *pi = port_find(p[i].handle);
                if (!pi) {
                    struct port_info tmp;
                    memset(&tmp, 0, sizeof tmp);
                    tmp.frames = 256;
                    tmp.rate = 48000;
                    tmp.format = ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO;
                    tee_buffer(&tmp, p[i].pointer);
                } else {
                    tee_buffer(pi, p[i].pointer);
                }
            }
        }
    }
    return HOOK_CONTINUE(sceAudioOutOutputs,
                         int32_t (*)(void *, uint32_t),
                         param, num);
}

static int install_hooks(void)
{
    if (g_active)
        return 0;
    memset(g_ports, 0, sizeof g_ports);
    g_first_hit_logged = 0;
    g_net_state = 0;
    g_sock = -1;

    /* GoldHEN SDK macros: HOOK_INIT at file scope, HOOK() installs 64-bit detour. */
    HOOK(sceAudioOutOpen);
    HOOK(sceAudioOutClose);
    HOOK(sceAudioOutOutput);
    HOOK(sceAudioOutOutputs);
    g_active = 1;
    write_loaded_marker();
    final_printf("[shell_audio_tap] hooks installed UDP 127.0.0.1:%d\n", PCM_PORT);
    return 0;
}

static void remove_hooks(void)
{
    if (!g_active)
        return;
    UNHOOK(sceAudioOutOutputs);
    UNHOOK(sceAudioOutOutput);
    UNHOOK(sceAudioOutClose);
    UNHOOK(sceAudioOutOpen);
    if (g_sock >= 0) {
        sceNetSocketClose(g_sock);
        g_sock = -1;
    }
    g_net_state = 0;
    g_active = 0;
    unlink(TAP_MARK);
    unlink(TAP_MARK_A);
    final_printf("[shell_audio_tap] hooks removed\n");
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    final_printf("[GoldHEN] %s 0x%08x plugin_load\n", g_pluginName, g_pluginVersion);
    return install_hooks();
}

int32_t attr_public plugin_unload(int32_t argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    remove_hooks();
    return 0;
}

/* Direct LoadStartModule / sys_sdk_proc_prx_load: only install hooks. */
s32 attr_module_hidden module_start(s64 argc, const void *args)
{
    (void)argc;
    (void)args;
    return install_hooks();
}

s32 attr_module_hidden module_stop(s64 argc, const void *args)
{
    (void)argc;
    (void)args;
    remove_hooks();
    return 0;
}
