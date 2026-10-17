/* SPDX-License-Identifier: GPL-3.0-or-later */
/* HearBridge PS4 web UI: shared status, command queue and HTTP server
 * (port 8090, like HearBridge PS5). The HTTP server runs in its own thread;
 * the daemon updates status through ui_* setters and polls commands with
 * ui_take_cmd(). All shared data is guarded by one tiny spinlock. */
#ifndef HEARBRIDGE_UI_H
#define HEARBRIDGE_UI_H

#include <stdint.h>

#define UI_PORT       8090
#define UI_PORT_ALT   8091   /* used when 8090 stays taken (old instance) */
#define UI_PORT_FILE  "/data/hearbridge/ui_port"
#define UI_SCAN_MAX   16
#define UI_LOG_MAX    24

enum ui_cmd_type {
    UI_CMD_NONE = 0,
    UI_CMD_SCAN,
    UI_CMD_CONNECT,     /* addr */
    UI_CMD_DISCONNECT,
    UI_CMD_FORGET,      /* addr */
    UI_CMD_STOP
};

struct ui_cmd {
    int type;
    unsigned char addr[6];
};

struct ui_scan_dev {
    unsigned char addr[6];
    uint32_t cod;
    int rssi;
    char name[64];
};

/* Start the HTTP thread (payload build only). 0 on success. */
/* Bind port and start the HTTP thread. 0 ok, else the errno of bind/listen. */
int  ui_start(const char *version, int port);
/* Close the listening socket and wait for the HTTP thread to end. */
void ui_stop(void);
int  ui_port(void);                    /* port in use, 0 if none */

/* Daemon -> UI */
void ui_set_state(const char *state);
void ui_set_device(const unsigned char addr[6], const char *name);   /* addr NULL: none */
void ui_set_stream(const char *codec, int rate, int bitpool, int bitpool_max,
                   int pcm_ms, int media_drops, int resyncs);
void ui_set_streaming(int on);
/* PCM from the game tap: packets received, seconds without PCM, tap title,
 * 1 = the tap is sending but nothing arrives here. */
void ui_set_pcm(unsigned long rx_pkts, long idle_s, const char *tap_title, int lost);
/* Short text of the last failure (a FAIL line), shown on the page until
 * streaming starts. */
void ui_set_error(const char *text);
void ui_set_scanning(int on);          /* 0 off, 1 inquiry (resets the timer), 2 asking names */
int  ui_cod_rank(uint32_t cod);        /* 0 headphones .. 3 TV / set-top box */
void ui_set_scan_results(const struct ui_scan_dev *d, int n);
void ui_log(const char *line);
/* Log ring for the /log page: each line is stored with the wall clock and
 * shown as HH:MM:SS. Small fixed memory (300 lines of 127 characters). */
void ui_logbuf_add(const char *line);

/* UI -> daemon */
int  ui_take_cmd(struct ui_cmd *out);  /* 1 if a command was pending */
int  ui_volume(void);                  /* boost, percent 0..500 (100 = unchanged) */
/* Audio loop: the page's equalizer (when on) and the Boost gain on
 * interleaved int16 stereo, gain_q8 from hb_gain_q8(). Same as hb_apply_gain
 * while the equalizer is off. */
void ui_eq_apply(int16_t *pcm, int frames, int32_t gain_q8);
/* AVRCP absolute volume of the headphones: avail 0 = not supported or not
 * connected; vol127 -1 = unknown. ui_take_hs_volume() returns a level the
 * page asked for (0..127) once, or -1. */
void ui_set_hs_volume(int avail, int vol127);
int  ui_take_hs_volume(void);
/* Console LAN address for the UI URL ("127.0.0.1" if unknown). */
void ui_local_ip(char *out, int outlen);

#endif
