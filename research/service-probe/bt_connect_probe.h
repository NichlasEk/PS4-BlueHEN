/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Own-fd diagnostic only. No PIN guessing or audio calls. Confirmation requires controller input.
 * ABI recovered from own 9.60 ShellUI wrappers and managed metadata. */
/* Query all records using the already tested 00.10 envelope, but expose only
 * the selected device's class/service/flag fields. Never log names or addresses. */
static int connection_snapshot(FILE *f,int fd,uint64_t address,const char *phase)
{
    struct bt_guarded_records records={.before=BT_CANARY,.after=BT_CANARY};
    struct bt_registered_query query={.address=0,.records=records.items,.capacity=32};
    errno=0;int rc=ioctl(fd,0x80186201UL,&query);int saved_errno=errno;
    int guards=records.before==BT_CANARY && records.after==BT_CANARY;
    int valid=guards && rc>=0 && rc<=32;unsigned matches=0;
    fprintf(f,"BT_CONNECT_RECORDS phase=%s result=%d errno=%d guards=%d\n",phase,rc,rc<0?saved_errno:0,guards);
    if(valid)for(unsigned i=0;i<32;i++) {
        const struct bt_registered_record *record=&records.items[i];
        if(record->address!=address)continue;
        matches++;
        fprintf(f,"BT_CONNECT_TARGET phase=%s cod=0x%08x services=0x%08x flags=0x%08x a2dp_sink=%u a2dp_source=%u hid=%u\n",
            phase,record->cod,record->services,record->flags,
            (record->services&8u)!=0,(record->services&4u)!=0,(record->services&0x40u)!=0);
    }
    fprintf(f,"BT_CONNECT_TARGET_COUNT phase=%s valid=%d count=%u NO_NAMES_OR_ADDRESSES\n",phase,valid,matches);
    memset(&records,0,sizeof(records));
    return valid && matches<=1?0:-1;
}

static int connection_confirm(FILE *f,int fd,uint64_t address)
{
    ui_pair_pending=1;ui_pair_decision=0;
    snprintf(ui_status,sizeof(ui_status),"Headsetet begar parkoppling. X = godkann, CIRKEL = avbryt.");
    for(unsigned tick=0;tick<400 && ui_running && !ui_cancel && !ui_pair_decision;tick++) {
        ui_frame();usleep(50000);
    }
    int accept=ui_pair_decision==1 && ui_running && !ui_cancel;
    ui_pair_pending=0;ui_pair_decision=0;
    struct {uint64_t address;uint32_t accept,reserved;} request={address,(uint32_t)accept,0};
    _Static_assert(sizeof(request)==16,"confirmation envelope");
    errno=0;int rc=ioctl(fd,0x80106208UL,&request);
    fprintf(f,"BT_CONNECT_CONFIRM accepted=%d result=%d errno=%d\n",accept,rc,rc<0?errno:0);
    snprintf(ui_status,sizeof(ui_status),accept?"Parkoppling godkand. Vantar pa headsetet...":"Parkoppling avbruten.");
    return rc==0?accept:-1;
}

/* Driver status is kept separate from local cleanup/transport failures. */
static int connection_driver_status=-1;
static int connection_hold(FILE *f,int fd,int queue,uint64_t address)
{
    ui_connected=1;
    snprintf(ui_status,sizeof(ui_status),"Ansluten. CIRKEL kopplar ned. Ljud/mikrofon ar inte aktiverade.");
    fprintf(f,"BT_CONNECTION_HELD NO_AUDIO\n");
    int result=0;
    unsigned ticks=0;
    while(ui_running && !ui_cancel) {
        struct bt_event event={0};int rc=next_event(queue,fd,&event);
        if(rc<0){result=61;break;}
        if(rc>0 && event.address==address) {
            fprintf(f,"BT_HELD_EVENT type=%u status=%u TARGET_ONLY\n",event.type,event.status);
            if(event.type==6){result=65;break;}
            if(event.type==5 && event.status){connection_driver_status=event.status;result=64;break;}
        }
        if(ticks++%20==0) {
            int state=ioctl(fd,0x80086203UL,&address);
            if(state<0){result=66;break;}
            if(state!=3 && state!=5 && state!=6){result=65;break;}
        }
        ui_frame();usleep(50000);
    }
    ui_connected=0;
    fprintf(f,"BT_CONNECTION_RELEASE result=%d user_requested=%d\n",result,ui_cancel || !ui_running);
    return result;
}

static int connection_probe(FILE *f,uint64_t address,uint32_t device_class)
{
    int fd=-1,queue=-1,attached=0,started=0,result=50,rc;
    unsigned events=0;int confirmation_replied=0,connect_succeeded=0;
    ui_cancel=0;ui_connected=0;connection_driver_status=-1;
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
    if(connection_snapshot(f,fd,address,"before")!=0){result=73;goto cleanup;}
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
                if(event.type==3){result=63;break;}
                if(event.type==4) {
                    if(confirmation_replied){result=72;break;}
                    confirmation_replied=1;
                    int answer=connection_confirm(f,fd,address);
                    if(answer<=0){result=answer<0?71:57;break;}
                }
                if(event.type==5) {
                    fprintf(f,"BT_CONNECT_RESULT status=%u value=0x%08x\n",event.status,event.value);
                    if(event.status){connection_driver_status=event.status;result=64;break;}
                    connect_succeeded=1;
                }
                if(event.type==6){result=65;break;}
            }
        }
        if(tick%20==0 || (rc>0 && event.address==address && event.type==5)) {
            errno=0;int state=ioctl(fd,0x80086203UL,&address);
            fprintf(f,"BT_CONNECT_STATE state=%d errno=%d\n",state,state<0?errno:0);
            /* 9.60 query 0x594bf0 returns 6 for an established audio link
             * whose stream bits are clear. Require this attempt's success event. */
            if(state==3 || state==5 || (state==6 && connect_succeeded)){result=connection_hold(f,fd,queue,address);break;}
            if(state<0){result=66;break;}
        }
        ui_frame();if(ui_cancel || !ui_running){result=57;break;}usleep(50000);
    }
 cleanup:
    if(started) {
        errno=0;int last_state=ioctl(fd,0x80086203UL,&address);
        fprintf(f,"BT_CONNECT_OUTCOME result=%d state=%d errno=%d\n",result,last_state,last_state<0?errno:0);
        int snapshot_ok=connection_snapshot(f,fd,address,"outcome")==0;
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
        else if(connection_snapshot(f,fd,address,"disconnected")!=0 || !snapshot_ok)result=74;
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
