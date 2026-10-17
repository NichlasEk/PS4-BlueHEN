#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Short id of the daemon sources, passed to the daemon and the launcher so the
# launcher can tell a running daemon of this build from an older one.
cd "$(dirname "$0")/.." || exit 1
{ ls src/*.c src/*.h | grep -v 'webui_html\.h\|webui_assets\.h'; ls src/web/*; } | LC_ALL=C sort | xargs cat | sha256sum | cut -c1-10
