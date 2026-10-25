/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include "probe.c"
static int loads, unloads, lookups, present, load_error, unload_error, lookup_error, inventory_error;
static uint32_t fw = 0x09600011;
int32_t sceKernelGetSystemSwVersion(struct sw_version *v) { v->version = fw; return 0; }
int32_t sceKernelGetModuleList(uint32_t *h, size_t cap, size_t *n)
{ assert(cap==256); h[0]=10; *n=present ? 1 : 0; return inventory_error; }
int32_t sceKernelGetModuleInfo(uint32_t h, struct module_info *v)
{ assert(h==10 && v->size==352); strcpy(v->name,"libSceAudioIn.sprx"); return 0; }
int32_t sceSysmoduleLoadModuleInternal(uint32_t id)
{ assert(id==AUDIOIN_ID); loads++; if (!load_error) present=1; return load_error; }
int32_t sceSysmoduleUnloadModuleInternal(uint32_t id)
{ assert(id==AUDIOIN_ID); unloads++; if (!unload_error) present=0; return unload_error; }
int32_t sceKernelDlsym(int32_t h, const char *n, void **p)
{ assert(h==10 && n==symbols[lookups++]); *p=(void *)(uintptr_t)1; return lookup_error; }
static void reset(void)
{ loads=unloads=lookups=present=load_error=unload_error=lookup_error=inventory_error=0; }
int main(void)
{
 FILE *log=tmpfile(); assert(log);
 fw=0x11000000; assert(bluehen_modules(log)==2 && loads==0); fw=0x09600011;
 assert(bluehen_modules(log)==0 && loads==1 && unloads==1 && lookups==3 && !present);
 reset(); present=1; assert(bluehen_modules(log)==8 && !loads && !unloads);
 reset(); inventory_error=-1; assert(bluehen_modules(log)==8 && !loads);
 reset(); load_error=-1; assert(bluehen_modules(log)==3 && loads==1 && !unloads);
 reset(); lookup_error=-1; assert(bluehen_modules(log)==4 && unloads==1 && !present);
 reset(); unload_error=-1; assert(bluehen_modules(log)==6 && unloads==1 && present);
 fclose(log); puts("sysmodule probe: firmware, ownership, lookup cleanup and failure tests PASS");
}
