#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Host-side 10 s stereo sweep to a PS4 HearBridge daemon.

The daemon binds 127.0.0.1:9500 on the console, so this script only works if
you have a way to inject into that localhost (normally use pcm_sweep_payload
on the console instead). Kept for lab setups that forward the port.
"""
import argparse
import math
import socket
import struct
import time

RATE = 48000
SECONDS = 10
CHUNK = 480
F0 = 200.0
F1 = 4000.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=9500)
    args = ap.parse_args()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    phase = 0.0
    total = RATE * SECONDS
    sent = 0
    while sent < total:
        n = min(CHUNK, total - sent)
        frames = []
        for i in range(n):
            t = (sent + i) / RATE
            f = F0 + (F1 - F0) * (t / SECONDS)
            s = int(8000.0 * math.sin(phase))
            frames.append(struct.pack("<hh", s, s))
            phase += 2 * math.pi * f / RATE
            if phase > 2 * math.pi:
                phase -= 2 * math.pi
        sock.sendto(b"".join(frames), (args.host, args.port))
        sent += n
        time.sleep(n / RATE)
    print(f"sent {sent} frames to {args.host}:{args.port}")


if __name__ == "__main__":
    main()
