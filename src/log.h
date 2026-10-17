/* SPDX-License-Identifier: GPL-3.0-or-later */
/* PS4 build: avrcp.c logs through the daemon log (log_line in probe.c). */
#ifndef HEARBRIDGE_LOG_H
#define HEARBRIDGE_LOG_H
void log_line(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif
