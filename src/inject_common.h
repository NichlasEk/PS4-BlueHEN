/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Shared helpers for HearBridge one-shot PRX injectors (GoldHEN BinLoader). */
#ifndef HEARBRIDGE_INJECT_COMMON_H
#define HEARBRIDGE_INJECT_COMMON_H

#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/syscall.h>
#include <dirent.h>
#include <errno.h>

#define HB_DIR          "/data/hearbridge"
#define HB_DIR_ALT      "/user/data/hearbridge"
#define STATE_PATH      HB_DIR "/state"
#define STATE_PATH_ALT  HB_DIR_ALT "/state"
#define INJECT_LOG      HB_DIR "/inject.log"
#define INJECT_LOG_ALT  HB_DIR_ALT "/inject.log"
#define TAP_LOADED      HB_DIR "/tap.loaded"
#define TAP_LOADED_ALT  HB_DIR_ALT "/tap.loaded"
#define TAP_LOG         HB_DIR "/tap.log"
#define TAP_LOG_ALT     HB_DIR_ALT "/tap.log"

#define GOLDHEN_SDK_CMD_VERSION          0
#define GOLDHEN_SDK_CMD_PROCESS_PRX_LOAD 6

struct proc_prx_load {
    char process_name[32];
    char prx_path[100];
    uint64_t res;
} __attribute__((packed));

struct notify_req {
    int32_t type, req_id, priority, msg_id, target_id, user_id, unk1, unk2, app_id, error_num, unk3;
    char use_icon_uri;
    char message[1024];
    char icon_uri[1024];
    char unk[1024];
};

int32_t sceKernelSendNotificationRequest(int32_t, void *, size_t, int32_t);

static FILE *g_inj_log;

static void inj_log_open(void)
{
    mkdir(HB_DIR, 0777);
    mkdir(HB_DIR_ALT, 0777);
    g_inj_log = fopen(INJECT_LOG, "w");
    if (!g_inj_log)
        g_inj_log = fopen(INJECT_LOG_ALT, "w");
}

static void inj_log(const char *fmt, ...)
{
    va_list ap;
    char line[512];
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    puts(line);
    if (g_inj_log) {
        fputs(line, g_inj_log);
        fputc('\n', g_inj_log);
        fflush(g_inj_log);
        fsync(fileno(g_inj_log));
    }
}

static void inj_log_close(void)
{
    if (g_inj_log) {
        fflush(g_inj_log);
        fsync(fileno(g_inj_log));
        fclose(g_inj_log);
        g_inj_log = NULL;
    }
}

static void notify(const char *fmt, ...)
{
    static struct notify_req r;
    va_list ap;
    memset(&r, 0, sizeof r);
    r.use_icon_uri = 1;
    snprintf(r.icon_uri, sizeof r.icon_uri, "cxml://psnotification/tex_icon_system");
    va_start(ap, fmt);
    vsnprintf(r.message, sizeof r.message, fmt, ap);
    va_end(ap);
    sceKernelSendNotificationRequest(0, &r, sizeof r, 0);
}

/* Raw GoldHEN syscall 500. The payload SDK's libc syscall() returns
 * -errno when the kernel sets the carry flag, and its int return type then
 * gets sign-extended, which is how the old version query printed
 * 0xffffffffffffff00 (-0x100). Here rax and the carry flag are kept apart.
 * Returns rax; *carry = 1 if the kernel flagged an error. */
static uint64_t goldhen_raw(uint64_t cmd, void *data, int *carry)
{
    register uint64_t rax __asm__("rax") = 500;
    register uint64_t rdi __asm__("rdi") = cmd;
    register uint64_t rsi __asm__("rsi") = (uint64_t)(uintptr_t)data;
    uint8_t cf;

    __asm__ volatile("syscall\n\tsetc %3"
                     : "+r"(rax), "+r"(rdi), "+r"(rsi), "=r"(cf)
                     :
                     : "rcx", "rdx", "r8", "r9", "r10", "r11", "memory", "cc");
    if (carry)
        *carry = cf;
    return rax;
}

/* GoldHEN SDK version as a 32-bit value (0x00000100 = 1.0.0).
 * GoldHEN may hand it back either as a normal return or through the error
 * path (carry set, rax = version), so take the low 32 bits of raw rax in
 * both cases. Returns 0 if syscall 500 is absent (ENOSYS) or reports 0. */
static uint32_t goldhen_version(void)
{
    int cf = 0;
    uint64_t rax = goldhen_raw(GOLDHEN_SDK_CMD_VERSION, NULL, &cf);
    uint32_t v = (uint32_t)rax;

    inj_log("goldhen: version raw rax=0x%llx carry=%d", (unsigned long long)rax, cf);
    if (cf && v == ENOSYS)
        return 0;
    inj_log("goldhen: sdk version 0x%08x (%u.%02x)", v, (v >> 8) & 0xffffff, v & 0xff);
    return v;
}

/* Read first token from state file (e.g. "streaming"). Returns 0 on success. */
static int read_state(char *out, size_t out_sz)
{
    FILE *f;
    char buf[64];
    size_t n;
    char *nl;

    f = fopen(STATE_PATH, "r");
    if (!f)
        f = fopen(STATE_PATH_ALT, "r");
    if (!f)
        return -1;
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (n == 0)
        return -1;
    buf[n] = '\0';
    nl = strchr(buf, '\n');
    if (nl)
        *nl = '\0';
    snprintf(out, out_sz, "%s", buf);
    return 0;
}

/* Find SceShellUI (home screen) via sysctl. Returns pid > 0 or -1.
 * No other process is ever considered: loading into SceShellCore crashed
 * the console. */
static int find_shellui(char *name_out, size_t name_sz)
{
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0 };
    size_t buf_size = 0;
    void *buf, *ptr;
    int found_pid = -1;

    if (name_out && name_sz)
        name_out[0] = '\0';

    if (sysctl(mib, 4, NULL, &buf_size, NULL, 0) != 0 || buf_size == 0)
        return -1;
    buf = malloc(buf_size);
    if (!buf)
        return -1;
    if (sysctl(mib, 4, buf, &buf_size, NULL, 0) != 0) {
        free(buf);
        return -1;
    }
    for (ptr = buf; (char *)ptr < (char *)buf + buf_size; ) {
        struct kinfo_proc *ki = (struct kinfo_proc *)ptr;
        if (ki->ki_structsize <= 0)
            break;
        ptr = (char *)ptr + ki->ki_structsize;
        if (strcmp(ki->ki_comm, "SceShellUI") == 0) {
            found_pid = (int)ki->ki_pid;
            if (name_out && name_sz)
                snprintf(name_out, name_sz, "%s", ki->ki_comm);
            break;
        }
    }
    free(buf);
    return found_pid;
}

/* Parse tap.loaded. Returns 1 if marker exists for the same live ShellUI pid. */
static int already_loaded(int shellui_pid, char *plugin_out, size_t plugin_sz)
{
    FILE *f;
    char line[128];
    char plugin[64] = "";
    int marked_pid = -1;

    f = fopen(TAP_LOADED, "r");
    if (!f)
        f = fopen(TAP_LOADED_ALT, "r");
    if (!f)
        return 0;
    while (fgets(line, sizeof line, f)) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';
        if (strncmp(line, "pid=", 4) == 0)
            marked_pid = atoi(line + 4);
        else if (plugin[0] == '\0' && line[0] != '\0')
            snprintf(plugin, sizeof plugin, "%s", line);
    }
    fclose(f);
    if (plugin_out && plugin_sz)
        snprintf(plugin_out, plugin_sz, "%s", plugin);
    if (marked_pid > 0 && shellui_pid > 0 && marked_pid == shellui_pid)
        return 1;
    return 0;
}

static int write_prx_bytes(const char *path, const unsigned char *data, unsigned int len)
{
    int fd;
    ssize_t w;
    struct stat st;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0)
        return -1;
    w = write(fd, data, len);
    if (w == (ssize_t)len)
        fsync(fd);
    close(fd);
    if (w != (ssize_t)len)
        return -1;
    chmod(path, 0777);
    if (stat(path, &st) != 0 || st.st_size != (off_t)len)
        return -1;
    return 0;
}

#define SHELLUI_PROC      "SceShellUI"
#define SANDBOX_ROOT      "/mnt/sandbox"
#define SHELLUI_TITLE     "NPXS20001"
#define GOLDHEN_PLUGINS   "/data/GoldHEN/plugins"

/* SceShellUI runs jailed in /mnt/sandbox/NPXS20001_xxx. /data is not mounted
 * there (Orbis Toolbox nullfs-mounts it before loading its sprx), which is
 * why /data/hearbridge/... gave 0x80020002. Find the sandbox root so the PRX
 * can be written into a directory ShellUI itself sees. Prefers _000. */
static int find_shellui_sandbox(char *out, size_t out_sz)
{
    DIR *d;
    struct dirent *e;
    char best[64] = "";

    d = opendir(SANDBOX_ROOT);
    if (!d) {
        inj_log("sandbox: cannot open %s errno=%d", SANDBOX_ROOT, errno);
        return -1;
    }
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, SHELLUI_TITLE "_", sizeof(SHELLUI_TITLE)) != 0)
            continue;
        inj_log("sandbox: found %s/%s", SANDBOX_ROOT, e->d_name);
        if (best[0] == '\0' || strcmp(e->d_name, SHELLUI_TITLE "_000") == 0)
            snprintf(best, sizeof best, "%s", e->d_name);
    }
    closedir(d);
    if (best[0] == '\0')
        return -1;
    snprintf(out, out_sz, "%s/%s", SANDBOX_ROOT, best);
    return 0;
}

static int is_dir(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Error codes meaning "ShellUI could not reach the file": try next path.
 * Anything else means the file was found and loading itself failed, so a
 * different path will not help and we stop. */
static int prx_path_unreachable(int64_t res)
{
    uint32_t r = (uint32_t)res;
    return r == 0x80020002u /* ENOENT */ || r == 0x80020001u /* EPERM */ ||
           r == 0x8002000du /* EACCES */ || r == 0x80020014u /* ENOTDIR */;
}

static int64_t load_into_shellui(const char *path)
{
    struct proc_prx_load args;
    int cf = 0;
    uint64_t rax;

    memset(&args, 0, sizeof args);
    strncpy(args.process_name, SHELLUI_PROC, sizeof args.process_name - 1);
    strncpy(args.prx_path, path, sizeof args.prx_path - 1);
    rax = goldhen_raw(GOLDHEN_SDK_CMD_PROCESS_PRX_LOAD, &args, &cf);
    inj_log("load: proc_prx_load(%s, %s) rax=0x%llx carry=%d res=0x%llx",
            SHELLUI_PROC, path, (unsigned long long)rax, cf,
            (unsigned long long)args.res);
    return (int64_t)args.res;
}

/* Copy the PRX to each candidate location and ask GoldHEN to load it into
 * SceShellUI, one at a time, stopping at the first success.
 *   1. /data/GoldHEN/plugins/<file>        (GoldHEN plugin folder)
 *   2. <sandbox>/system_tmp/<file> -> /system_tmp/<file>
 *   3. <sandbox>/tmp/<file>        -> /tmp/<file>
 *   4. <sandbox>/<file>            -> /<file>
 * <sandbox> = /mnt/sandbox/NPXS20001_000 (ShellUI's jail root). Writing
 * through the sandbox path guarantees ShellUI sees the same file at the
 * in-jail path. Returns the module handle (>= 0) or a negative error. */
static int64_t inject_prx_shellui(const char *file, const unsigned char *data,
                                  unsigned int len, char *used, size_t used_sz)
{
    static const char *const sbx_sub[] = { "/system_tmp", "/tmp", "" };
    char sbx[96];
    char real[192], view[192];
    int64_t res = (int64_t)(int32_t)0x80020002u;
    int have_sbx;
    unsigned i;

    if (used && used_sz)
        used[0] = '\0';

    /* 1. GoldHEN plugin folder */
    mkdir("/data/GoldHEN", 0777);
    mkdir(GOLDHEN_PLUGINS, 0777);
    snprintf(real, sizeof real, "%s/%s", GOLDHEN_PLUGINS, file);
    if (write_prx_bytes(real, data, len) == 0) {
        inj_log("try 1: wrote %s (%u bytes)", real, len);
        res = load_into_shellui(real);
        if (res >= 0) {
            if (used && used_sz)
                snprintf(used, used_sz, "%s", real);
            return res;
        }
        if (!prx_path_unreachable(res))
            return res;
    } else {
        inj_log("try 1: write %s failed errno=%d", real, errno);
    }

    /* 2-4. inside ShellUI's sandbox */
    have_sbx = find_shellui_sandbox(sbx, sizeof sbx) == 0;
    if (!have_sbx) {
        inj_log("sandbox: %s_* not found, no more paths", SHELLUI_TITLE);
        return res;
    }
    for (i = 0; i < sizeof sbx_sub / sizeof sbx_sub[0]; i++) {
        char dir[160];
        snprintf(dir, sizeof dir, "%s%s", sbx, sbx_sub[i]);
        if (!is_dir(dir)) {
            inj_log("try %u: %s missing, skip", i + 2, dir);
            continue;
        }
        snprintf(real, sizeof real, "%s/%s", dir, file);
        snprintf(view, sizeof view, "%s/%s", sbx_sub[i], file);
        if (write_prx_bytes(real, data, len) != 0) {
            inj_log("try %u: write %s failed errno=%d", i + 2, real, errno);
            continue;
        }
        inj_log("try %u: wrote %s (ShellUI sees %s)", i + 2, real, view);
        res = load_into_shellui(view);
        if (res >= 0) {
            if (used && used_sz)
                snprintf(used, used_sz, "%s", view);
            return res;
        }
        unlink(real);
        if (!prx_path_unreachable(res))
            return res;
    }
    return res;
}

#endif /* HEARBRIDGE_INJECT_COMMON_H */
