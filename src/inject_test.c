/* SPDX-License-Identifier: GPL-3.0-or-later */
/* inject_test - safe one-shot installer for the minimal shell_tap_test.prx.
 *
 * Same guards as inject_shell_audio (streaming state, locate ShellUI,
 * refuse if already loaded). The test PRX installs no audio hooks; it only
 * writes tap.log / tap.loaded so we can prove PROCESS_PRX_LOAD is safe.
 */
#include "inject_common.h"
#include "shell_tap_test_prx.h"

#define PLUGIN_NAME_FILE "shell_tap_test.prx"
#define PLUGIN_NAME  "shell_tap_test"

int main(void)
{
    char state[64];
    char proc_name[40];
    char already_name[64];
    char used_path[192];
    int shell_pid;
    int64_t res;
    uint32_t ver;

    inj_log_open();
    inj_log("inject_test: start (plugin=%s)", PLUGIN_NAME);

    inj_log("step: goldhen_sdk");
    ver = goldhen_version();
    if (ver == 0) {
        notify("HearBridge: GoldHEN SDK missing");
        inj_log("result: FAIL goldhen_sdk");
        inj_log_close();
        return 1;
    }

    inj_log("step: check_state");
    if (read_state(state, sizeof state) != 0) {
        notify("HearBridge: no daemon state (start HearBridge first)");
        inj_log("state: (missing)");
        inj_log("result: FAIL no_state");
        inj_log_close();
        return 1;
    }
    inj_log("state: %s", state);
    if (strcmp(state, "streaming") != 0) {
        notify("HearBridge: refuse inject (state=%s)", state);
        inj_log("result: FAIL state_not_streaming");
        inj_log_close();
        return 1;
    }

    inj_log("step: locate_shellui");
    shell_pid = find_shellui(proc_name, sizeof proc_name);
    if (shell_pid <= 0) {
        notify("HearBridge: SceShellUI not found");
        inj_log("shellui: not found");
        inj_log("result: FAIL no_shellui");
        inj_log_close();
        return 1;
    }
    inj_log("shellui: name=%s pid=%d", proc_name, shell_pid);

    inj_log("step: check_already_loaded");
    if (already_loaded(shell_pid, already_name, sizeof already_name)) {
        notify("HearBridge: tap already loaded (pid %d)", shell_pid);
        inj_log("already: yes plugin=%s pid=%d", already_name, shell_pid);
        inj_log("result: FAIL already_loaded");
        inj_log_close();
        return 1;
    }
    inj_log("already: no");

    inj_log("step: load (SceShellUI only)");
    notify("HearBridge: injecting ShellUI test PRX...");
    res = inject_prx_shellui(PLUGIN_NAME_FILE, shell_tap_test_prx, shell_tap_test_prx_len,
                             used_path, sizeof used_path);

    if (res < 0) {
        notify("HearBridge: test inject failed 0x%08x", (unsigned)res);
        inj_log("result: FAIL load res=0x%08x (no other process tried)", (unsigned)res);
        inj_log_close();
        return 1;
    }

    notify("HearBridge: test PRX loaded (check tap.log)");
    inj_log("result: OK handle=0x%llx path=%s", (unsigned long long)res, used_path);
    inj_log_close();
    return 0;
}
