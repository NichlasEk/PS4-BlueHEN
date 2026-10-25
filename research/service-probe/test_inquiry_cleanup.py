"""Exercise inquiry cleanup against a fake driver; no console or radio access."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class InquiryCleanup(unittest.TestCase):
    def test_driver_failure_boundaries(self):
        cc = shutil.which('cc')
        if not cc:
            self.skipTest('host C compiler unavailable')
        source = pathlib.Path(__file__).with_name('main_normal_inquiry.c').read_text()
        core = source[source.index('struct bt_event {'):source.index('static void inventory(')]
        prelude = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#define BT_CANARY UINT64_C(0xd97a04cf61b235e8)
static int mode,active,starts,stops,closes,deletes,reads;
static int mock_open(const char *p,int f) {(void)p;assert(f==(O_RDWR|O_NONBLOCK));return 7;}
static ssize_t mock_write(int f,const void *p,size_t n) {
 assert(f==7 && n==8 && *(const uint64_t *)p==0xfe);return mode==4?4:8;
}
static ssize_t mock_read(int f,void *p,size_t n) {
 assert(f==7 && n==16);if(mode==3){errno=EIO;return -1;}
 memset(p,0,n);((uint8_t *)p)[0]=active?(reads++?2:1):1;
 return 16;
}
static int mock_ioctl(int f,unsigned long request,int arg) {
 assert(f==7 && arg==0);
 if(request==0x20046204UL) {starts++;if(mode==1)return -1;active=1;return 0;}
 assert(request==0x20046205UL && active);stops++;active=0;return mode==5?-1:0;
}
static int mock_close(int fd) {assert(fd==7 || fd==8);closes++;return 0;}
static int mock_sleep(unsigned int usec) {assert(usec==50000);return 0;}
#define open mock_open
#define write mock_write
#define read mock_read
#define ioctl mock_ioctl
#define close mock_close
#define usleep mock_sleep
'''
        driver = r'''
static int mock_kqueue(void) {return 8;}
static int mock_kevent(int q,const struct bt_kevent *change,int n,
                      struct bt_kevent *event,int count,const struct bt_timeout *timeout) {
 assert(q==8);
 if(n) {assert(n==1 && change->ident==7 && change->filter==-1);
        if(change->flags==2)deletes++;else assert(change->flags==1);return 0;}
 assert(count==1 && timeout && !timeout->sec && !timeout->nsec);
 if(!active && mode!=2)return 0;
 memset(event,0,sizeof(*event));event->ident=7;event->filter=-1;return 1;
}
int main(void) {
 const int expected[]={0,26,25,27,21,30};
 FILE *log=tmpfile();assert(log);
 bt_kqueue=mock_kqueue;bt_kevent=mock_kevent;
 for(mode=0;mode<6;mode++) {
  active=starts=stops=closes=deletes=reads=0;
  int rc=inquiry(log);assert(rc==expected[mode]);
  assert(starts==(mode==2 || mode==4?0:1));
  assert(stops==(mode==0 || mode==3 || mode==5?1:0));
  assert(closes==(mode==4?1:2));assert(deletes==(mode==4?0:1));
 }
 fclose(log);return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory)
            (path/'check.c').write_text(prelude+core+driver)
            subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-variable', str(path/'check.c'),
                            '-o', str(path/'check')], check=True, capture_output=True)
            subprocess.run([str(path/'check')], check=True, timeout=5)
