/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#define BLUEHEN_MAIN_HOST_TEST
#include "main_vmic.c"
int32_t sceKernelGetSystemSwVersion(struct sw_version *v) { (void)v; return -1; }
int32_t sceSysmoduleLoadModuleInternal(uint32_t id) { (void)id; return -1; }
int32_t sceSysmoduleUnloadModuleInternal(uint32_t id) { (void)id; return -1; }
int32_t sceKernelGetModuleList(uint32_t *p,size_t n,size_t *o) { (void)p;(void)n;(void)o;return -1; }
int32_t sceKernelGetModuleInfo(uint32_t h,struct module_info *p) { (void)h;(void)p;return -1; }
int32_t sceKernelDlsym(int32_t h,const char *n,void **p) { (void)h;(void)n;(void)p;return -1; }
int main(void) {
 assert(valid_cleanup(123,123,0x40100,0x40100));
 assert(valid_cleanup(123,123,0x40203,(int32_t)0x809b00ff));
 assert(!valid_cleanup(123,999,0x40100,0x40100));
 assert(!valid_cleanup(123,123,0x40101,0x40100));
 assert(!valid_cleanup(123,123,0x40104,-1));
 assert(!valid_cleanup(123,123,0,-1));
 assert(!valid_cleanup(123,123,0x140100,-1));
 puts("main-thread supervisor cleanup guard tests PASS");
}
