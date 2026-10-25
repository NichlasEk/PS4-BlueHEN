"""Exercise selected-device connection ownership/cleanup with a fake driver."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class ConnectProbe(unittest.TestCase):
    def test_connection_boundaries(self):
        cc = shutil.which('cc')
        if not cc:
            self.skipTest('C compiler unavailable')
        source = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "bt_registered.h"
struct bt_event {uint8_t type,status,reserved[2];uint32_t value;uint64_t address;};
struct bt_kevent {uint64_t ident;int16_t filter;uint16_t flags;uint32_t fflags;int64_t data;void *udata;};
static int mode,active,started,ended,closed,deleted,ui_cancel,ui_running=1,frames;
static int ui_pair_pending,ui_pair_decision,confirmations;
static char ui_status[128];
static const uint64_t target=0x123456;
static int mock_open(const char *p,int flags) {(void)p;assert(flags==(O_RDWR|O_NONBLOCK));return 7;}
static ssize_t mock_write(int fd,const void *p,size_t n) {assert(fd==7 && n==8 && *(const uint64_t*)p==0xfe);return n;}
static int mock_close(int fd) {assert(fd==7 || fd==8);closed++;return 0;}
static int mock_sleep(unsigned int us) {assert(us==50000);return 0;}
static int mock_ioctl(int fd,unsigned long request,const void *data) {
 assert(fd==7);
 if(request==0x80186201UL) {
  const struct bt_registered_query *q=data;assert(q->address==0 && q->capacity==32);
  if(mode==16 || (mode==19 && started))return -1;
  if(mode==17){q->records[32-1].reserved1[102]=0;((uint64_t *)(q->records+32))[0]=0;return 1;}
  if(mode==18)return 33;
  q->records[0].address=0xabcdef;q->records[0].services=0x40;
  if(started){q->records[1].address=target;q->records[1].services=8;return 2;}
  return 1;
 }
 const uint64_t *addr=data;assert(*addr==target);
 if(request==0x80106208UL){const uint32_t *words=data;confirmations++;assert(words[2]==(mode==13?0:1) && words[3]==0);return mode==14?-1:0;}
 if(request==0x80086203UL) {
  if(!started)return mode==1?3:1;
  if(ended)return mode==8?2:1;
  return mode==0 || mode==11 || mode==19 || (mode==12 && confirmations)?3:2;
 }
 if(request==0x8008620aUL) {started++;if(mode==2){errno=EIO;return -1;}active=1;return 0;}
 assert(request==0x8008620bUL && active);ended++;active=0;return 0;
}
static int bt_kqueue(void) {return 8;}
static int bt_kevent(int q,const struct bt_kevent *c,int n,void *e,int count,void *t) {
 (void)e;(void)t;assert(q==8 && n==1 && count==0 && c->ident==7);
 if(c->flags==2)deleted++;else assert(c->flags==1);return 0;
}
static int next_event(int q,int fd,struct bt_event *e) {
 assert(q==8 && fd==7);
 if(!active){if(mode==9){e->type=1;return 1;}return 0;}
 if(mode==3)return -1;
 e->address=target;
 if(mode==4){e->type=3;return 1;}
 if(mode>=12 && mode<=15){e->type=4;return 1;}
 if(mode==5){e->type=5;e->status=5;return 1;}
 if(mode==11){e->type=5;e->status=5;e->address=0xabcdef;return 1;}
 return 0;
}
static void ui_frame(void) {frames++;if(mode==6 && active)ui_cancel=1;if(mode==10)ui_cancel=1;if(ui_pair_pending){if(mode==13)ui_cancel=1;else ui_pair_decision=1;}}
#define open mock_open
#define write mock_write
#define close mock_close
#define usleep mock_sleep
#define ioctl mock_ioctl
#include "bt_connect_probe.h"
int main(void) {
 FILE *f=tmpfile();assert(f);
 const int expected[]={0,58,59,61,63,64,57,60,67,56,57,0,0,57,71,72,73,73,73,74};
 for(mode=0;mode<20;mode++) {
  active=started=ended=closed=deleted=ui_cancel=frames=confirmations=ui_pair_pending=ui_pair_decision=0;
  assert(connection_probe(f,target,0x240404)==expected[mode]);
  int owns=mode!=1 && mode!=2 && mode!=9 && mode!=10 && (mode<16 || mode==19);
  assert(confirmations==(mode>=12 && mode<=15?1:0));assert(ended==owns);assert(closed==2 && deleted==1);
 }
 started=ended=closed=0;
 assert(connection_probe(f,target,0x2508)==51);
 assert(connection_probe(f,0,0x404)==51);
 assert(!started && !ended && !closed);
 rewind(f);char logs[65536];size_t n=fread(logs,1,sizeof(logs)-1,f);logs[n]=0;
 assert(!strstr(logs,"hid=1"));assert(strstr(logs,"phase=before valid=1 count=0"));
 assert(strstr(logs,"phase=outcome valid=1 count=1"));
 fclose(f);return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p = pathlib.Path(tmp)
            (p/'probe.c').write_text(source)
            subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-I', str(pathlib.Path(__file__).parent),
                            str(p/'probe.c'), '-o', str(p/'probe')],
                           check=True, capture_output=True)
            subprocess.run([str(p/'probe')], check=True, timeout=5)
