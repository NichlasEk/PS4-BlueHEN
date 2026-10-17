/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Small SDP server (A2DP Source + Device ID records). Pure byte-in /
 * byte-out. From HearBridge-PS5 src/a2dp/sdp_server.h. */
#ifndef HEARBRIDGE_SDP_SERVER_H
#define HEARBRIDGE_SDP_SERVER_H

/* Handle one SDP request PDU; writes the response PDU. Returns its length,
 * or 0 if nothing should be sent. rsp_max should be >= the channel MTU. */
int sdp_server_handle(const unsigned char *req, int len,
                      unsigned char *rsp, int rsp_max);

#endif
