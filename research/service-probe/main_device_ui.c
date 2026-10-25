/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bounded own-descriptor inquiry. No pairing, connection or audio. */
#define BLUEHEN_MODULE_HOST_TEST
#include "../module-probe/probe.c"
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "bt_registered.h"
#include <SDL2/SDL.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include <stb/stb_easy_font.h>
#pragma clang diagnostic pop

struct bt_event { uint8_t type,status,reserved[2]; uint32_t value; uint64_t address; };
struct bt_kevent { uint64_t ident; int16_t filter; uint16_t flags; uint32_t fflags; int64_t data; void *udata; };
struct bt_timeout { int64_t sec,nsec; };
_Static_assert(sizeof(struct bt_event)==16,"event ABI");
_Static_assert(sizeof(struct bt_kevent)==32,"kevent ABI");
static int (*bt_kqueue)(void);
static int (*bt_kevent)(int,const struct bt_kevent *,int,struct bt_kevent *,int,const struct bt_timeout *);
static FILE *diag_log;

/* Poll only our descriptor, with the native zero-timeout kevent ABI. */
static int next_event(int queue,int fd,struct bt_event *event)
{
    struct bt_kevent ready={0};
    const struct bt_timeout timeout={0};
    int rc=bt_kevent(queue,NULL,0,&ready,1,&timeout);
    if(rc<0 && errno==EINTR) return 0;
    if(rc<=0) return rc;
    if(rc!=1 || ready.ident!=(uint64_t)fd || ready.filter!=-1 ||
       (ready.flags&(0x4000u|0x8000u))) return -2;
    struct { uint64_t before; struct bt_event event; uint64_t after; } guarded={BT_CANARY,{0},BT_CANARY};
    ssize_t n=read(fd,&guarded.event,sizeof(guarded.event));
    if(n<0 && (errno==EAGAIN || errno==EINTR)) return 0;
    if(n!=sizeof(guarded.event) || guarded.before!=BT_CANARY || guarded.after!=BT_CANARY) return -3;
    *event=guarded.event;
    return 1;
}

static int permit(const char *path,int pid);
struct ui_device { uint64_t address; char name[121]; };
static struct ui_device ui_devices[32];
static unsigned ui_count;
static int ui_focus,ui_selected=-1,ui_running=1,ui_scanning,ui_cancel,ui_want_scan;
static SDL_Window *ui_window;
static SDL_Renderer *ui_renderer;
static int ui_pad=-1;
static uint32_t ui_last_buttons,ui_last_input;
static char ui_status[128]="Tryck TRIANGEL for att soka efter enheter.";

static void ui_box(int x,int y,int w,int h,int r,int g,int b)
{
    SDL_Rect rect={x,y,w,h};SDL_SetRenderDrawColor(ui_renderer,r,g,b,255);SDL_RenderFillRect(ui_renderer,&rect);
}
static void ui_text(int x,int y,int scale,const char *label,int r,int g,int b)
{
    struct vertex { float x,y,z; unsigned char rgba[4]; } vertices[8192];
    int quads=stb_easy_font_print(0,0,(char *)label,NULL,vertices,sizeof(vertices));
    SDL_SetRenderDrawColor(ui_renderer,r,g,b,255);
    for(int i=0;i<quads;i++) {
        float xmin=vertices[i*4].x,xmax=xmin,ymin=vertices[i*4].y,ymax=ymin;
        for(int j=1;j<4;j++) {struct vertex v=vertices[i*4+j];if(v.x<xmin)xmin=v.x;if(v.x>xmax)xmax=v.x;if(v.y<ymin)ymin=v.y;if(v.y>ymax)ymax=v.y;}
        SDL_Rect rect={x+(int)(xmin*scale),y+(int)(ymin*scale),(int)((xmax-xmin)*scale),(int)((ymax-ymin)*scale)};
        SDL_RenderFillRect(ui_renderer,&rect);
    }
}
static void ui_frame(void)
{
    SDL_Event event;while(SDL_PollEvent(&event))if(event.type==SDL_QUIT){ui_running=0;ui_cancel=1;}
    OrbisPadData pad={0};
    if(ui_pad>=0 && scePadReadState(ui_pad,&pad)==0) {
        uint32_t pressed=pad.buttons&~ui_last_buttons;ui_last_buttons=pad.buttons;
        if(pressed)ui_last_input=SDL_GetTicks();
        if(pressed&ORBIS_PAD_BUTTON_OPTIONS){ui_running=0;ui_cancel=1;}
        if(pressed&ORBIS_PAD_BUTTON_CIRCLE){if(ui_scanning)ui_cancel=1;else ui_running=0;}
        if(!ui_scanning) {
            if((pressed&ORBIS_PAD_BUTTON_UP) && ui_focus>0)ui_focus--;
            if((pressed&ORBIS_PAD_BUTTON_DOWN) && ui_focus+1<(int)ui_count)ui_focus++;
            if((pressed&ORBIS_PAD_BUTTON_CROSS) && ui_count){ui_selected=ui_focus;fprintf(diag_log,"UI_SELECTED slot=%d\n",ui_selected);}
            if(pressed&ORBIS_PAD_BUTTON_TRIANGLE)ui_want_scan=1;
        }
    }
    if(SDL_GetTicks()-ui_last_input>300000u){ui_running=0;ui_cancel=1;}
    if(permit("/data/bluehen/service-ui-v13.stop",(int)getpid())){ui_running=0;ui_cancel=1;}
    ui_box(0,0,1280,720,10,17,30);ui_box(0,0,1280,5,56,174,245);
    ui_text(55,42,4,"BlueHEN",235,244,255);ui_text(58,104,2,"Bluetooth-enheter",130,170,200);
    ui_text(58,153,2,ui_status,210,225,240);
    int first=ui_focus>=6?ui_focus-5:0;
    if(!ui_count) {ui_text(80,260,2,"Inga enheter i listan.",180,200,220);ui_text(80,305,2,"Satt headsetet i parkopplingslage och sok.",140,165,190);}
    for(int row=0;row<6 && first+row<(int)ui_count;row++) {
        int i=first+row,y=220+row*58;char label[64];snprintf(label,sizeof(label),"%.48s%s",ui_devices[i].name,ui_selected==i?"  [VALD]":"");
        ui_box(55,y,760,48,i==ui_focus?25:17,i==ui_focus?84:30,i==ui_focus?125:47);
        ui_text(74,y+15,2,label,230,240,250);
    }
    ui_box(850,220,375,345,17,30,47);ui_text(875,245,2,"Vald utrustning",140,190,225);
    if(ui_selected>=0 && ui_selected<(int)ui_count) {
        char name[30];snprintf(name,sizeof(name),"%.25s",ui_devices[ui_selected].name);ui_text(875,296,2,name,240,245,255);
    } else ui_text(875,296,2,"Ingen enhet vald",180,195,210);
    ui_text(875,357,2,"Inte ansluten",245,185,100);
    ui_text(875,410,2,"Parkoppling kommer",150,170,190);ui_text(875,439,2,"i nasta steg.",150,170,190);
    ui_text(875,495,2,"Ljud / mikrofon:",150,170,190);ui_text(875,524,2,"inte klara annu",150,170,190);
    ui_text(58,636,2,"UPP/NER  Navigera    X  Valj    TRIANGEL  Sok",210,225,240);
    ui_text(58,678,2,ui_scanning?"CIRKEL  Avbryt sokning":"CIRKEL / OPTIONS  Avsluta",140,165,190);
    SDL_RenderPresent(ui_renderer);
    int present_rc=SDL_UpdateWindowSurface(ui_window);
    static int first_frame;
    if(!first_frame){fprintf(diag_log,"UI_FRAME_PRESENTED result=%d\n",present_rc);first_frame=1;}
    if(present_rc<0){fprintf(diag_log,"UI_PRESENT_FAILED %s\n",SDL_GetError());ui_running=0;ui_cancel=1;}
}
static int ui_names(FILE *f,int fd)
{
    for(unsigned i=0;i<ui_count && ui_running;i++) {
        struct {uint64_t before;unsigned char name[128];uint64_t after;} buffer={.before=BT_CANARY,.after=BT_CANARY};
        struct {uint64_t address;unsigned char *name;} request={ui_devices[i].address,buffer.name};
        _Static_assert(sizeof(request)==16,"name envelope");
        int rc=ioctl(fd,0xc0106206UL,&request);
        int guards=buffer.before==BT_CANARY && buffer.after==BT_CANARY;
        for(unsigned j=121;j<128;j++)if(buffer.name[j])guards=0;
        fprintf(f,"BT_NAME slot=%u result=%d guards=%d\n",i,rc,guards);
        if(!guards){ui_running=0;ui_cancel=1;return 34;}
        if(rc>=0 && buffer.name[0] && memchr(buffer.name,0,121)) {
            unsigned j;for(j=0;j<120 && buffer.name[j];j++)ui_devices[i].name[j]=(buffer.name[j]>=32 && buffer.name[j]<127)?buffer.name[j]:'?';
            ui_devices[i].name[j]=0;
        }
        memset(&buffer,0,sizeof(buffer));ui_frame();
    }
    return 0;
}

static int inquiry(FILE *f)
{
    int status=20,fd=-1,queue=-1,attached=0,started=0;
    unsigned events=0,results=0,unique=0; uint64_t addresses[32]={0};
    memset(ui_devices,0,sizeof(ui_devices));ui_count=0;ui_focus=0;ui_selected=-1;ui_cancel=0;
    fd=open("/dev/bt",O_RDWR|O_NONBLOCK);
    fprintf(f,"BT_OPEN ok=%d errno=%d\n",fd>=0,fd<0?errno:0);
    if(fd<0) goto cleanup;
    /* Same eight-byte subscription request as ShellUI. Ownership/coexistence
     * still requires physical verification; never use ShellUI's descriptor. */
    const uint64_t subscription=0xfe;
    ssize_t n=write(fd,&subscription,sizeof(subscription));
    fprintf(f,"BT_SUBSCRIBE bytes=%lld errno=%d\n",(long long)n,n<0?errno:0);
    if(n!=sizeof(subscription)) {status=21;goto cleanup;}
    queue=bt_kqueue();
    if(queue<0) {status=22;goto cleanup;}
    struct bt_kevent change={.ident=(uint64_t)fd,.filter=-1,.flags=1};
    int rc=bt_kevent(queue,&change,1,NULL,0,NULL);
    fprintf(f,"BT_QUEUE_ADD result=%d\n",rc);
    if(rc<0) {status=23;goto cleanup;}
    attached=1;
    /* Do not issue stop for someone else's discovery operation. Reject
     * inquiry traffic during the passive phase. This cannot prove exclusivity. */
    for(unsigned tick=0;tick<40;tick++) {
        struct bt_event event={0}; rc=next_event(queue,fd,&event);
        if(rc<0) {status=24;goto cleanup;}
        if(rc>0) {
            fprintf(f,"BT_PASSIVE_EVENT type=%u status=%u\n",event.type,event.status);
            if(event.type==1 || event.type==2) {status=25;goto cleanup;}
        }
        ui_frame();if(ui_cancel || !ui_running){status=0;goto cleanup;}
        usleep(50000);
    }
    fprintf(f,"BT_INQUIRY_BEGIN limit_ticks=300 tick_us=50000\n");
    errno=0;rc=ioctl(fd,0x20046204UL,0);
    fprintf(f,"BT_INQUIRY_START result=%d errno=%d\n",rc,rc<0?errno:0);
    if(rc!=0) {status=26;goto cleanup;}
    started=1;status=0;
    for(unsigned tick=0;tick<300;tick++) {
        struct bt_event event={0};rc=next_event(queue,fd,&event);
        if(rc<0) {status=27;break;}
        if(rc>0) {
            if(++events>128) {status=28;break;}
            if(event.type==1) {
                results++;
                unsigned slot=0;
                while(slot<unique && addresses[slot]!=event.address) slot++;
                if(slot==unique && event.address && unique<32) {
                    addresses[unique++]=event.address;ui_devices[ui_count].address=event.address;
                    snprintf(ui_devices[ui_count].name,sizeof(ui_devices[ui_count].name),"Enhet %u (namn okant)",ui_count+1);ui_count++;
                }
                fprintf(f,"BT_INQUIRY_RESULT slot=%u status=%u value=0x%08x address_present=%d\n",slot,event.status,event.value,event.address!=0);
            } else {
                fprintf(f,"BT_EVENT type=%u status=%u\n",event.type,event.status);
            }
            if(event.type==2) {
                fprintf(f,"BT_INQUIRY_STOP_EVENT status=%u\n",event.status);
                if(event.status) status=29;
                break;
            }
        }
        ui_frame();if(ui_cancel || !ui_running)break;
        usleep(50000);
    }
 cleanup:
    if(started) {
        errno=0;int stop_rc=ioctl(fd,0x20046205UL,0);
        fprintf(f,"BT_INQUIRY_STOP result=%d errno=%d\n",stop_rc,stop_rc<0?errno:0);
        if(stop_rc!=0) status=30;
        else if(ui_running && !ui_cancel) {snprintf(ui_status,sizeof(ui_status),"Hamtar enhetsnamn...");ui_frame();int name_status=ui_names(f,fd);if(name_status)status=name_status;}
    }
    if(attached) {
        struct bt_kevent change={.ident=(uint64_t)fd,.filter=-1,.flags=2};
        int del_rc=bt_kevent(queue,&change,1,NULL,0,NULL);
        fprintf(f,"BT_QUEUE_DELETE result=%d\n",del_rc);
        if(del_rc<0 && status==0) status=31;
    }
    if(queue>=0) {int rc=close(queue);fprintf(f,"BT_QUEUE_CLOSE result=%d\n",rc);if(rc && status==0) status=32;}
    if(fd>=0) {int rc=close(fd);fprintf(f,"BT_CLOSE result=%d\n",rc);if(rc && status==0) status=33;}
    memset(addresses,0,sizeof(addresses));
    fprintf(f,"BT_SUMMARY events=%u inquiry_results=%u unique=%u NO_NAMES_OR_ADDRESSES\n",events,results,unique);
    return status;
}

static int gui_run(FILE *f)
{
    int outcome=0,sdl_attempted=0;unsigned loaded=0;
    /* IDs from OpenOrbis _types/sysmodule.h; own-process reference counts. */
    const uint32_t dependencies[]={0x80000011u,0x80000022u,0x80000024u};
    for(unsigned i=0;i<3;i++) {
        fprintf(f,"UI_MODULE_LOAD_BEGIN id=0x%08x\n",dependencies[i]);
        int rc=sceSysmoduleLoadModuleInternal(dependencies[i]);
        fprintf(f,"UI_MODULE_LOAD_END result=0x%08x\n",(unsigned)rc);
        if(rc){outcome=43;goto ui_cleanup;}loaded++;
    }
    fprintf(f,"UI_SDL_INIT_BEGIN\n");sdl_attempted=1;
    int init_rc=SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
    fprintf(f,"UI_SDL_INIT_END result=%d\n",init_rc);
    if(init_rc!=0){fprintf(f,"UI_INIT_FAILED %s\n",SDL_GetError());outcome=40;goto ui_cleanup;}
    fprintf(f,"UI_WINDOW_BEGIN\n");
    ui_window=SDL_CreateWindow("BlueHEN",0,0,1920,1080,0);
    fprintf(f,"UI_WINDOW_END ok=%d\n",ui_window!=NULL);
    if(!ui_window){outcome=41;goto ui_cleanup;}
    fprintf(f,"UI_SURFACE_BEGIN\n");SDL_Surface *surface=SDL_GetWindowSurface(ui_window);
    fprintf(f,"UI_SURFACE_END ok=%d\n",surface!=NULL);
    if(!surface){outcome=42;goto ui_cleanup;}
    ui_renderer=SDL_CreateSoftwareRenderer(surface);
    fprintf(f,"UI_RENDERER_END ok=%d\n",ui_renderer!=NULL);
    if(!ui_renderer){outcome=42;goto ui_cleanup;}
    if(SDL_RenderSetLogicalSize(ui_renderer,1280,720)<0){outcome=44;goto ui_cleanup;}
    fprintf(f,"UI_PAD_INIT_BEGIN\n");
    int user=-1;sceUserServiceInitialize(NULL);sceUserServiceGetForegroundUser(&user);
    if(user<0)sceUserServiceGetInitialUser(&user);
    if(user>=0 && scePadInit()>=0)ui_pad=scePadOpen(user,ORBIS_PAD_PORT_TYPE_STANDARD,0,NULL);
    fprintf(f,"UI_READY pad_ok=%d\n",ui_pad>=0);ui_last_input=SDL_GetTicks();
    if(ui_pad<0)snprintf(ui_status,sizeof(ui_status),"Ingen handkontroll. Avslutar automatiskt om fem minuter.");
    while(ui_running) {
        ui_frame();
        if(ui_want_scan && ui_running) {
            ui_want_scan=0;ui_scanning=1;snprintf(ui_status,sizeof(ui_status),"Soker enheter... CIRKEL avbryter.");
            int rc=inquiry(f);ui_scanning=0;ui_last_input=SDL_GetTicks();
            fprintf(f,"UI_SCAN_END result=%d count=%u\n",rc,ui_count);
            if(rc==0)snprintf(ui_status,sizeof(ui_status),"%u enheter hittades. Valj utrustning med UPP/NER och X.",ui_count);
            else snprintf(ui_status,sizeof(ui_status),"Sokningen kunde inte slutforas. Resultat %d.",rc);
            if(rc==30 || rc==34){outcome=rc;ui_running=0;fprintf(f,"UI_EXIT cleanup/integrity failure; no retry\n");}
        }
        SDL_Delay(33);
    }
 ui_cleanup:
    if(ui_pad>=0)scePadClose(ui_pad);
    memset(ui_devices,0,sizeof(ui_devices));
    if(ui_renderer)SDL_DestroyRenderer(ui_renderer);
    if(ui_window)SDL_DestroyWindow(ui_window);
    if(sdl_attempted)SDL_Quit();
    while(loaded) {
        uint32_t id=dependencies[--loaded];int rc=sceSysmoduleUnloadModuleInternal(id);
        fprintf(f,"UI_MODULE_UNLOAD id=0x%08x result=0x%08x\n",id,(unsigned)rc);
        if(rc && outcome==0)outcome=45;
    }
    fprintf(f,"UI_CLOSED result=%d\n",outcome);return outcome;
}

static void inventory(void)
{
    uint32_t handles[256]; size_t count=0;
    int rc=sceKernelGetModuleList(handles,256,&count);
    fprintf(diag_log,"INVENTORY rc=%d count=%zu\n",rc,count);
    if(rc || count>256) return;
    for(size_t i=0;i<count;i++) {
        struct module_info info={.size=sizeof(info)};
        rc=sceKernelGetModuleInfo(handles[i],&info);
        if(!rc) fprintf(diag_log,"MODULE handle=%u name=%.256s\n",handles[i],info.name);
    }
}

static void *resolve(const char *module, const char *symbol)
{
    uint32_t handles[256]; size_t count=0;
    if (sceKernelGetModuleList(handles,256,&count) || count>256) return NULL;
    for (size_t i=0;i<count;i++) {
        struct module_info info={.size=sizeof(info)};
        if (sceKernelGetModuleInfo(handles[i],&info)) continue;
        if (!strcmp(info.name,module)) {
            void *p=NULL;
            int rc=sceKernelDlsym(handles[i],symbol,&p);
            fprintf(diag_log,"LOOKUP module=%s symbol=%s rc=0x%08x resolved=%d\n",module,symbol,(unsigned)rc,p!=NULL);
            if (!rc) return p;
        }
    }
    return NULL;
}
static int permit(const char *path,int pid)
{
    FILE *f=fopen(path,"r"); if (!f) return 0;
    int candidate=-1; int n=fscanf(f,"%d",&candidate); fclose(f);
    return n==1 && candidate==pid;
}
int main(void)
{
    int (*exit_app)(const char *,const char **)=NULL;
    int pid=(int)getpid(),status=1,system_owned=0;
    FILE *f=fopen("/data/bluehen/service-ui-v13.log","a");
    if (!f) return 1;
    setvbuf(f,NULL,_IONBF,0);
    diag_log=f;
    struct sw_version version={.size=sizeof(version)};
    int rc=sceKernelGetSystemSwVersion(&version);
    fprintf(f,"SERVICE_UI_V13 pid=%d firmware_rc=0x%08x firmware=0x%08x\n",pid,(unsigned)rc,version.version);
    if (rc || (version.version&0xffff0000u)!=0x09600000u) goto done;
    inventory();
    const char *kernels[]={"libkernel.sprx","libkernel_sys.sprx","libkernel_web.sprx","libkernel","libkernel_sys","libkernel_web"};
    int (*auth)(int,void *)=NULL; int (*parent)(void)=NULL;
    for (unsigned i=0;i<sizeof(kernels)/sizeof(kernels[0]);i++) {
        if (!auth) auth=resolve(kernels[i],"get_authinfo");
        if (!parent) parent=resolve(kernels[i],"getppid");
        if (!bt_kqueue) bt_kqueue=resolve(kernels[i],"kqueue");
        if (!bt_kevent) bt_kevent=resolve(kernels[i],"kevent");
    }
    if (!auth) {fprintf(f,"REFUSED unresolved get_authinfo\n");goto done;}
    uint64_t info[32]={0}; rc=auth(pid,info);
    fprintf(f,"IDENTITY pid=%d ppid=%d auth_rc=%d system_bit=%u\n",pid,parent?parent():-1,rc,rc==0?(unsigned)((info[1]>>62)&1):0);
    int profile_match=rc==0 && !memcmp(info,bluehen_expected_auth,sizeof(bluehen_expected_auth));
    fprintf(f,"AUTH_PROFILE_MATCH value=%d\n",profile_match);
    memset(info,0,sizeof(info));
    if (!profile_match) {status=4;goto done;}
    /* OpenOrbis _types/sysmodule.h: INTERNAL_SYSTEM_SERVICE = 0x80000010.
     * Acquire only in this process after the complete auth profile matches. */
    fprintf(f,"SYSTEM_LOAD_BEGIN id=0x80000010\n");
    rc=sceSysmoduleLoadModuleInternal(0x80000010u);
    fprintf(f,"SYSTEM_LOAD_END rc=0x%08x\n",(unsigned)rc);
    if (rc) {status=6;goto done;}
    system_owned=1;
    inventory();
    exit_app=resolve("libSceSystemService.sprx","sceSystemServiceLoadExec");
    if (!exit_app || !bt_kqueue || !bt_kevent) {status=8;goto done;}
    fprintf(f,"QUEUE_READY kqueue=%p kevent=%p\n",(void *)bt_kqueue,(void *)bt_kevent);
    fprintf(f,"READY pid=%d auth=%p exit=%p NO_DAEMON_REGISTER\n",pid,(void *)auth,(void *)exit_app);
    unsigned waited;
    for (waited=0;waited<60;waited++) {
        if (permit("/data/bluehen/service-ui-v13.allow",pid)) break;
        sleep(1);
    }
    if (waited==60) {fprintf(f,"REFUSED permission timeout\n");goto done;}
    status=gui_run(f);
 done:
    fprintf(f,"RESULT result=%d NO_AUDIO NO_PAIR NO_CONNECT NO_DISCONNECT\n",status);
    if (system_owned && exit_app) {
        fprintf(f,"EXIT_SYSTEM_BEGIN status=%d\n",status);
        rc=exit_app("exit",NULL);
        fprintf(f,"EXIT_SYSTEM_RETURN result=0x%08x\n",(unsigned)rc);
        sleep(3);
        fprintf(f,"EXIT_FALLBACK process still running\n");
        status=9;
    }
    if (system_owned) {
        fprintf(f,"SYSTEM_UNLOAD_BEGIN\n");
        rc=sceSysmoduleUnloadModuleInternal(0x80000010u);
        fprintf(f,"SYSTEM_UNLOAD_END rc=0x%08x\n",(unsigned)rc);
        if (rc) status=7;
    }
    fprintf(f,"END result=%d NO_AUDIO NO_PAIR NO_CONNECT NO_DISCONNECT NO_CREDENTIAL_CHANGES\n",status);
    fclose(f);return status;
}
