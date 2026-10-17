/* SPDX-License-Identifier: GPL-3.0-or-later */
/* HCI event framing check. The event reader hands the daemon a byte stream
 * and events are cut out of it by their length byte. If one USB packet is
 * ever lost the cut is off by a few bytes and every later event is garbage
 * (Inquiry Complete or the Inquiry_Cancel answer would never be seen).
 * Events with a fixed length are checked here; when the header at the
 * start does not fit, one byte is dropped and the stream finds itself again.
 * Header only, host tests include it. */
#ifndef HEARBRIDGE_HCIFRAME_H
#define HEARBRIDGE_HCIFRAME_H

/* 1 if (event code, parameter length) can be a real HCI event. */
static inline int hci_evt_plausible(unsigned code, unsigned len)
{
    if (code == 0 || (code > 0x3E && code != 0xFF)) return 0;
    switch (code) {
    case 0x01: return len == 1;                       /* Inquiry Complete */
    case 0x03: return len == 11;                      /* Connection Complete */
    case 0x04: return len == 10;                      /* Connection Request */
    case 0x05: return len == 4;                       /* Disconnection Complete */
    case 0x06: return len == 3;                       /* Authentication Complete */
    case 0x07: return len == 255;                     /* Remote Name Request Complete */
    case 0x08: return len == 4;                       /* Encryption Change */
    case 0x0E: return len >= 3;                       /* Command Complete */
    case 0x0F: return len == 4;                       /* Command Status */
    case 0x13: return len >= 5 && (len - 1) % 4 == 0; /* Number Of Completed Packets */
    case 0x17: return len == 6;                       /* Link Key Request */
    case 0x18: return len == 23;                      /* Link Key Notification */
    case 0x22: return len >= 15 && (len - 1) % 14 == 0; /* Inquiry Result with RSSI */
    case 0x31: return len == 6;                       /* IO Capability Request */
    case 0x36: return len == 7;                       /* Simple Pairing Complete */
    default:   return 1;
    }
}

/* A Connection Request event is the code, the length byte (10) and 10
 * parameter bytes (address 6, class 3, link type 1): 12 bytes in all.
 * Asking for more would drop the saved headset's own call unanswered. */
#define HCI_EVT_CONN_REQUEST_LEN 12

static inline int hci_conn_request_ok(const unsigned char *ev, int n)
{
    return n >= HCI_EVT_CONN_REQUEST_LEN && ev[0] == 0x04 && ev[1] == 10;
}

/* Create_Connection refused with Connection Already Exists (0x0B) or
 * Command Disallowed (0x0C): the controller is busy with a call from the
 * headset or already has the link. That is not a failure, wait for it. */
static inline int hci_create_conn_busy(unsigned status)
{
    return status == 0x0B || status == 0x0C;
}

#endif
