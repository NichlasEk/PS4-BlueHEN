/* SPDX-License-Identifier: GPL-3.0-or-later */
/* pcm_stop - send ASCII "stop" to the HearBridge daemon on UDP 127.0.0.1:9500. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef HEARBRIDGE_PAYLOAD
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#include <orbis/Net.h>
#include <orbis/_types/net.h>
#endif

int32_t sceKernelSendNotificationRequest(int32_t, void *, size_t, int32_t);

struct notify_req {
    int32_t type, req_id, priority, msg_id, target_id, user_id, unk1, unk2, app_id, error_num, unk3;
    char use_icon_uri;
    char message[1024];
    char icon_uri[1024];
    char unk[1024];
};

static void notify(const char *msg)
{
    static struct notify_req r;
    memset(&r, 0, sizeof r);
    r.use_icon_uri = 1;
    snprintf(r.icon_uri, sizeof r.icon_uri, "cxml://psnotification/tex_icon_system");
    snprintf(r.message, sizeof r.message, "%s", msg);
    sceKernelSendNotificationRequest(0, &r, sizeof r, 0);
}

int main(void)
{
#ifdef HEARBRIDGE_PAYLOAD
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a;
    const char *cmd = "stop";
    if (s < 0) return 1;
    memset(&a, 0, sizeof a);
    a.sin_len = sizeof a;
    a.sin_family = AF_INET;
    a.sin_port = htons(9500);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sendto(s, cmd, 4, 0, (struct sockaddr *)&a, sizeof a);
    close(s);
#else
    struct {
        uint8_t sin_len, sin_family;
        uint16_t sin_port;
        OrbisNetInAddr sin_addr;
        char sin_zero[8];
    } a;
    int s = sceNetSocket("hb_stop", ORBIS_NET_AF_INET, ORBIS_NET_SOCK_DGRAM, 0);
    if (s < 0) return 1;
    memset(&a, 0, sizeof a);
    a.sin_len = sizeof a;
    a.sin_family = ORBIS_NET_AF_INET;
    a.sin_port = __builtin_bswap16(9500);
    a.sin_addr.s_addr = __builtin_bswap32(0x7f000001u);
    sceNetSendto(s, "stop", 4, 0, (const OrbisNetSockaddr *)&a, sizeof a);
    sceNetSocketClose(s);
#endif
    notify("HearBridge: stop sent");
    puts("stop sent");
    return 0;
}
