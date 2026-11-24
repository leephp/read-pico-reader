#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Actual file-manager counts with the sliced directory walker; no list/result cap."""
from pathlib import Path
import os,subprocess,tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "file_search.h"
#include <stdatomic.h>
#include <pthread.h>
#ifdef TEST_WORKER
#define ESP_PLATFORM 1
#define pdPASS 1
static char s_book_count_root[288];
static atomic_bool s_book_count_running, s_book_count_cancel;
static bool s_book_count_started, s_book_count_error;
static pthread_t ui_thread;
static atomic_bool hold_io;
static bool create_failed;
static char count_category_paths[2][288];static int category_counts[2];
static atomic_bool dir_running,entered_io;
static void cancel_directory(void){}
static bool read_pico_sd_begin_listing(void){
 atomic_store(&entered_io,true);
 while(atomic_load(&hold_io)) usleep(100);
 return true;
}
static void read_pico_sd_end_listing(void){}
static bool read_pico_sd_io_ready(void){
 assert(!pthread_equal(ui_thread,pthread_self()));
 while(atomic_load(&hold_io)&&!atomic_load(&s_book_count_cancel)) usleep(100);
 return !atomic_load(&s_book_count_cancel);
}
static void vTaskDelay(int ticks){(void)ticks;usleep(100);}
static void vTaskDelete(void *t){(void)t;}
static void file_book_count_worker(void *);
static void *run_worker(void *arg){file_book_count_worker(arg);return NULL;}
static int xTaskCreate(void (*fn)(void *),const char *n,int sz,void *arg,int pr,void *out){
 (void)n;(void)sz;(void)pr;(void)out;assert(fn==file_book_count_worker);
 pthread_t t;if(create_failed||pthread_create(&t,NULL,run_worker,arg))return 0;
 pthread_detach(t);return pdPASS;
}
#else
static file_search_t *s_book_count_search;
#endif
static file_search_t *s_file_search;
static int s_book_count_total,s_counts[3];
static bool s_book_count_dirty,s_book_count_incomplete;
static struct{bool mounted;}s_sd={true};
static const char *root;
static const char *folder_root(int folder){(void)folder;return root;}
'''
for name in ['supported','file_book_count_invalidate','file_book_count_visit','file_book_count_pending','file_book_count_tick','file_count_label','count_root','file_search_stop']:
    if name=='file_book_count_pending':
        u+='#ifdef TEST_WORKER\n'+function('file_book_count_worker',R/'main/apps/app_files.c')+'\n#endif\n'
    u+=function(name,R/'main/apps/app_files.c')+'\n'
u+=r'''
static void finish(void){unsigned steps=0;while(!file_book_count_tick()){assert(++steps<10000);usleep(100);}assert(!file_book_count_pending());}
int main(int argc,char **argv){
#ifdef TEST_WORKER
 ui_thread=pthread_self();
#endif
 assert(argc==3);root=argv[1];char label[48];s_counts[0]=999;
 assert(count_root(0)==999&&s_book_count_dirty);file_count_label(0,label,sizeof(label));assert(!strcmp(label,"统计中"));
 finish();assert(s_counts[0]==653&&!s_book_count_incomplete);file_count_label(0,label,sizeof(label));assert(!strcmp(label,"653 本"));
 assert(count_root(0)==653);
#ifdef TEST_WORKER
 atomic_store(&entered_io,false);atomic_store(&hold_io,true);
#endif
 assert(!file_book_count_tick()&&file_book_count_pending());
#ifdef TEST_WORKER
 while(!atomic_load(&entered_io)) usleep(100);
#endif
 assert(!file_book_count_tick());file_search_stop();assert(s_book_count_dirty);
#ifdef TEST_WORKER
 assert(atomic_load(&s_book_count_running));atomic_store(&hold_io,false);
#endif
 finish();assert(s_counts[0]==653);
 char path[512];snprintf(path,sizeof(path),"%s/root.TXT",root);assert(!unlink(path));count_root(0);finish();assert(s_counts[0]==652);
 s_sd.mounted=false;assert(count_root(0)==0);s_sd.mounted=true;
 root=argv[2];count_root(0);finish();assert(s_counts[0]==0&&s_book_count_incomplete);file_count_label(0,label,sizeof(label));assert(!strcmp(label,"0+ 本"));
 root="/does/not/exist";count_root(0);finish();assert(s_counts[0]==0&&s_book_count_incomplete);
#ifdef TEST_WORKER
 create_failed=true;count_root(0);assert(file_book_count_tick()&&s_book_count_incomplete);
#endif
 file_search_stop();assert(s_book_count_dirty);
 puts("PASS: recursive653 books across multiple levels and beyond96/512 caps; regular TXT/EPUB only, hidden/AppleDouble/symlink/folder exclusion, sliced completion/cancellation/recount, removed card and explicit incomplete-depth/read errors");
}
'''
with tempfile.TemporaryDirectory(prefix='book-count-') as d:
 p=Path(d);root=p/'card';root.mkdir();deep=p/'deep';deep.mkdir()
 for i in range(650):
  parent=root/str(i%7)/'second'/'third';parent.mkdir(parents=True,exist_ok=True);(parent/f'{i}.epub').touch()
 for rel in ['root.TXT','0/second/同名.EPUB','1/second/同名.EPUB','font.ttf','picture.png','._ghost.txt','.hidden.txt']:(root/rel).touch()
 (root/'folder.epub').mkdir();(root/'.hidden').mkdir();(root/'.hidden/secret.epub').touch();(root/'cycle').symlink_to(root,target_is_directory=True)
 nested=deep
 for _ in range(35):nested=nested/'d';nested.mkdir()
 (nested/'too-deep.epub').touch();(p/'test.c').write_text(u)
 for flags in ([],['-DTEST_WORKER','-pthread']):
  (root/'root.TXT').touch()
  subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize='+os.environ.get('HOST_TEST_SANITIZERS','address,undefined'),*flags,'-I'+str(R/'main/apps'),'-I'+str(R/'components/read_pico_search/include'),str(p/'test.c'),str(R/'main/apps/file_search.c'),str(R/'components/read_pico_search/read_pico_search.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test'),str(root),str(deep)],check=True,timeout=20)
