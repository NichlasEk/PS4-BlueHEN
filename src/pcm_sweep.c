/* SPDX-License-Identifier: GPL-3.0-or-later */
/* pcm_sweep - send a 10 s stereo frequency sweep to the HearBridge daemon.
 *
 * UDP 127.0.0.1:9500, raw interleaved s16le @ 48 kHz.
 * Run AFTER the daemon shows "streaming (UDP :9500)".
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#ifdef HEARBRIDGE_PAYLOAD
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#else
#include <orbis/Net.h>
#include <orbis/_types/net.h>
#endif

#define PORT 9500
#define RATE 48000
#define SECONDS 10
#define CHUNK_FRAMES 480 /* 10 ms */
#define F0 200.0
#define F1 4000.0

int32_t sceKernelSendNotificationRequest(int32_t, void *, size_t, int32_t);
int32_t sceKernelUsleep(uint32_t);

struct notify_req {
    int32_t type, req_id, priority, msg_id, target_id, user_id, unk1, unk2, app_id, error_num, unk3;
    char use_icon_uri;
    char message[1024];
    char icon_uri[1024];
    char unk[1024];
};

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

/* Quarter-wave polynomial; good enough for an audible sweep test. */
static double cheap_sin(double x)
{
    const double pi = 3.14159265358979323846;
    const double twopi = 2.0 * pi;
    double y, y2;
    while (x < -pi) x += twopi;
    while (x > pi) x -= twopi;
    y = x;
    y2 = y * y;
    return y * (1.0 - y2 * (1.0 / 6.0 - y2 * (1.0 / 120.0 - y2 / 5040.0)));
}

static void fill_chunk(int16_t *out, int frames, long n0, double *phase)
{
    int i;
    for (i = 0; i < frames; i++) {
        double t = (double)(n0 + i) / (double)RATE;
        double f = F0 + (F1 - F0) * (t / (double)SECONDS);
        int16_t s = (int16_t)(8000.0 * cheap_sin(*phase));
        out[i * 2] = out[i * 2 + 1] = s;
        *phase += 6.28318530717958647692 * f / (double)RATE;
        if (*phase > 6.28318530717958647692)
            *phase -= 6.28318530717958647692;
    }
}

#ifdef HEARBRIDGE_PAYLOAD
static int send_all(void)
{
    int s, total = RATE * SECONDS, sent_frames = 0;
    struct sockaddr_in a;
    int16_t buf[CHUNK_FRAMES * 2];
    double phase = 0;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        printf("socket failed\n");
        return -1;
    }
    memset(&a, 0, sizeof a);
    a.sin_len = sizeof a;
    a.sin_family = AF_INET;
    a.sin_port = htons(PORT);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    notify("HearBridge: PCM sweep %ds", SECONDS);
    while (sent_frames < total) {
        int n = total - sent_frames;
        if (n > CHUNK_FRAMES) n = CHUNK_FRAMES;
        fill_chunk(buf, n, sent_frames, &phase);
        if (sendto(s, buf, (size_t)n * 4u, 0, (struct sockaddr *)&a, sizeof a) < 0)
            break;
        sent_frames += n;
        sceKernelUsleep(10000);
    }
    close(s);
    notify("HearBridge: sweep done (%d frames)", sent_frames);
    printf("sent %d / %d frames\n", sent_frames, total);
    return sent_frames == total ? 0 : -1;
}
#else
struct hb_sockaddr_in {
    uint8_t sin_len;
    uint8_t sin_family;
    uint16_t sin_port;
    OrbisNetInAddr sin_addr;
    char sin_zero[8];
};
static int send_all(void)
{
    int s, total = RATE * SECONDS, sent_frames = 0;
    struct hb_sockaddr_in a;
    int16_t buf[CHUNK_FRAMES * 2];
    double phase = 0;

    s = sceNetSocket("hb_sweep", ORBIS_NET_AF_INET, ORBIS_NET_SOCK_DGRAM, 0);
    if (s < 0) return -1;
    memset(&a, 0, sizeof a);
    a.sin_len = sizeof a;
    a.sin_family = ORBIS_NET_AF_INET;
    a.sin_port = __builtin_bswap16((uint16_t)PORT);
    a.sin_addr.s_addr = __builtin_bswap32(0x7f000001u);
    notify("HearBridge: PCM sweep %ds", SECONDS);
    while (sent_frames < total) {
        int n = total - sent_frames;
        if (n > CHUNK_FRAMES) n = CHUNK_FRAMES;
        fill_chunk(buf, n, sent_frames, &phase);
        if (sceNetSendto(s, buf, (size_t)n * 4u, 0,
                         (const OrbisNetSockaddr *)&a, sizeof a) < 0)
            break;
        sent_frames += n;
        sceKernelUsleep(10000);
    }
    sceNetSocketClose(s);
    notify("HearBridge: sweep done (%d frames)", sent_frames);
    return sent_frames == total ? 0 : -1;
}
#endif

int main(void)
{
    return send_all();
}
