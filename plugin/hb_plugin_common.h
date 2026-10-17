/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Minimal helpers matching the GoldHEN plugin conventions. */
#pragma once
#include <stdint.h>
#include <string.h>
#include <Common.h>

#define attr_public __attribute__((visibility("default")))
#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define final_printf(a, args...) klog("(%s:%d) " a, __FILE__, __LINE__, ##args)

#define HB_ICON_SYSTEM "cxml://psnotification/tex_icon_system"

/* Same fields and call as GoldHEN's NotifyStatic (OSM-Made PS4-Notify). */
static inline void hb_notify(const char *text)
{
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof req);
    req.type = NotificationRequest;
    req.unk3 = 0;
    req.useIconImageUri = 1;
    req.targetId = -1;
    strncpy(req.message, text, sizeof req.message - 1);
    strncpy(req.iconUri, HB_ICON_SYSTEM, sizeof req.iconUri - 1);
    sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
}

/* GoldHEN fills struct proc_info through syscall 500. Use an oversized,
 * zeroed static buffer so a larger kernel-side struct cannot overrun the
 * stack. Copies the title ID (always NUL-terminated) into out. */
static inline int hb_title_id(char *out, size_t outlen)
{
    static union { struct proc_info pi; uint8_t pad[4096]; } u;
    int rc;
    memset(&u, 0, sizeof u);
    rc = sys_sdk_proc_info(&u.pi);
    if (outlen == 0)
        return rc;
    out[0] = 0;
    if (rc == 0 && u.pi.titleid[0]) {
        size_t n = sizeof u.pi.titleid;
        if (n > outlen - 1)
            n = outlen - 1;
        memcpy(out, u.pi.titleid, n);
        out[n] = 0;
    }
    return rc;
}
