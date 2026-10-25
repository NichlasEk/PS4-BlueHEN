/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Own-fd diagnostic only. No PIN guessing, confirmation reply or audio calls.
 * ABI recovered from own 9.60 ShellUI wrappers and managed metadata. */
static int connection_probe(FILE *f,uint64_t address,uint32_t device_class)
{
    int fd=-1,queue=-1,attached=0,started=0,result=50,rc;
    unsigned events=0;
    ui_cancel=0;
    if(!address || (device_class&0x1f00u)!=0x400u)return 51;
    fd=open("/dev/bt",O_RDWR|O_NONBLOCK);
    if(fd<0)goto cleanup;
    const uint64_t subscription=0xfe;
    if(write(fd,&subscription,sizeof(subscription))!=sizeof(subscription)){result=52;goto cleanup;}
    queue=bt_kqueue();if(queue<0){result=53;goto cleanup;}
    struct bt_kevent change={.ident=(uint64_t)fd,.filter=-1,.flags=1};
    if(bt_kevent(queue,&change,1,NULL,0,NULL)<0){result=54;goto cleanup;}attached=1;
    /* Never change an existing connection or compete with observed inquiry. */
    for(unsigned tick=0;tick<40;tick++) {
        struct bt_event event={0};rc=next_event(queue,fd,&event);
        if(rc<0){result=55;goto cleanup;}
        if(rc>0 && (event.type==1 || event.type==2 || event.address==address)){result=56;goto cleanup;}
        ui_frame();if(ui_cancel || !ui_running){result=57;goto cleanup;}usleep(50000);
    }
    errno=0;rc=ioctl(fd,0x80086203UL,&address);
    fprintf(f,"BT_CONNECT_BEFORE state=%d errno=%d\n",rc,rc<0?errno:0);
    /* Metadata: disconnected=1, connecting=2, connected=3, streaming=5. */
    if(rc!=1){result=58;goto cleanup;}
    errno=0;rc=ioctl(fd,0x8008620aUL,&address);
    fprintf(f,"BT_CONNECT_START result=%d errno=%d\n",rc,rc<0?errno:0);
    if(rc!=0){result=59;goto cleanup;}started=1;result=60;
    for(unsigned tick=0;tick<600;tick++) {
        struct bt_event event={0};rc=next_event(queue,fd,&event);
        if(rc<0){result=61;break;}
        if(rc>0) {
            if(++events>256){result=62;break;}
            if(event.address==address) {
                fprintf(f,"BT_CONNECT_EVENT type=%u status=%u TARGET_ONLY\n",event.type,event.status);
                if(event.type==3 || event.type==4){result=63;break;}
                if(event.type==5 && event.status){result=64;break;}
                if(event.type==6){result=65;break;}
            }
        }
        if(tick%20==0 || (rc>0 && event.address==address && event.type==5)) {
            errno=0;int state=ioctl(fd,0x80086203UL,&address);
            fprintf(f,"BT_CONNECT_STATE state=%d errno=%d\n",state,state<0?errno:0);
            if(state==3 || state==5){result=0;break;}
            if(state<0){result=66;break;}
        }
        ui_frame();if(ui_cancel || !ui_running){result=57;break;}usleep(50000);
    }
 cleanup:
    if(started) {
        /* Only this explicitly selected, initially disconnected address.
         * A registration created by pairing is retained; never delete records. */
        errno=0;int end=ioctl(fd,0x8008620bUL,&address);
        fprintf(f,"BT_CONNECT_DISCONNECT result=%d errno=%d\n",end,end<0?errno:0);
        int disconnected=0;
        for(unsigned tick=0;tick<100;tick++) {
            int state=ioctl(fd,0x80086203UL,&address);
            if(state==1){disconnected=1;break;}
            if(state<0)break;
            ui_frame();usleep(50000);
        }
        fprintf(f,"BT_CONNECT_CLEAN disconnected=%d\n",disconnected);
        if(!disconnected)result=67;
    }
    if(attached) {
        struct bt_kevent change={.ident=(uint64_t)fd,.filter=-1,.flags=2};
        rc=bt_kevent(queue,&change,1,NULL,0,NULL);
        fprintf(f,"BT_CONNECT_QUEUE_DELETE result=%d\n",rc);if(rc<0)result=68;
    }
    if(queue>=0){rc=close(queue);fprintf(f,"BT_CONNECT_QUEUE_CLOSE result=%d\n",rc);if(rc)result=69;}
    if(fd>=0){rc=close(fd);fprintf(f,"BT_CONNECT_FD_CLOSE result=%d\n",rc);if(rc)result=70;}
    fprintf(f,"BT_CONNECT_END result=%d events=%u\n",result,events);
    return result;
}
