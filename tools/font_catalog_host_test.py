#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：真实字体扫描的128款边界、双目录去重与索引内存。/ English: Real font scanning at128 entries, cross-directory deduplication and metadata memory."""
from pathlib import Path
import subprocess
import tempfile
import json
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
source = root / 'main/font/ttf_font.c'
unit = r'''
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include "font_filename.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "ttf_font.h"
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static size_t allocated;static unsigned allocations;
static void* heap_caps_calloc(size_t n,size_t bytes,unsigned caps){
 assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));allocated=n*bytes;++allocations;return calloc(n,bytes);
}
static const char *k_font_dirs[2],*user_dir;
static const char *app_settings_fonts_dir(void){return user_dir;}
static ttf_font_item_t *s_catalog;static int s_catalog_n;
static unsigned opens, reads, closes;
static DIR *counted_open(const char *path){++opens;return opendir(path);}
static struct dirent *counted_read(DIR *dir){assert(s_catalog_n<TTF_FONT_MAX);++reads;return readdir(dir);}
static int counted_close(DIR *dir){++closes;return closedir(dir);}
#define opendir counted_open
#define readdir counted_read
#define closedir counted_close
'''
for name in ('font_basename', 'is_junk_name', 'is_ttf_name', 'font_stem', 'font_item_cmp',
             'catalog_has_name', 'catalog_alloc', 'ttf_font_scan', 'ttf_font_count', 'ttf_font_item'):
    unit += function(name, source) + '\n'
unit += 'static void test_names(void){char out[96];\n'
for path in ('/fonts/中文字体.TTF', 'A.ttf', '.ttf', 'plain',
             '/字体/' + 'é中𠀀' * 13 + '.ttf', '字体' * 40 + '.ttf'):
    stem = path.rsplit('/', 1)[-1]
    if stem.lower().endswith('.ttf'):
        stem = stem[:-4]
    for capacity in range(81):
        expected = stem.encode()[:max(0, capacity - 1)].decode('utf-8', errors='ignore')
        if len(stem.encode()) >= capacity >= 7:
            head = stem.encode()[:(capacity - 4)//2].decode('utf-8', errors='ignore')
            tail = stem.encode()[-(capacity - 4 - len(head.encode())):].decode('utf-8', errors='ignore')
            expected = head + '…' + tail
        unit += f'memset(out, 0x5a, sizeof(out));font_stem({json.dumps(path, ensure_ascii=False)},out,{capacity});'
        if capacity:
            unit += f'assert(!strcmp(out,{json.dumps(expected, ensure_ascii=False)}));'
        unit += f'for(unsigned i={capacity};i<sizeof(out);++i)assert(out[i]==0x5a);\n'
unit += '}\n'
unit += r'''
#undef opendir
#undef readdir
#undef closedir
static void put(const char *dir,const char *name){
 char path[512];assert(snprintf(path,sizeof(path),"%s/%s",dir,name)<(int)sizeof(path));
 FILE *f=fopen(path,"wb");assert(f);assert(!fclose(f));
}
static void clear(const char *dir){
 DIR *d=opendir(dir);assert(d);struct dirent *ent;
 while((ent=readdir(d))){if(ent->d_name[0]=='.'&&(!ent->d_name[1]||!strcmp(ent->d_name,"..")))continue;
  char path[512];snprintf(path,sizeof(path),"%s/%s",dir,ent->d_name);assert(!unlink(path));}
 assert(!closedir(d));
}
int main(int argc,char **argv){
 test_names();
 assert(argc==3&&TTF_FONT_MAX==128&&sizeof(ttf_font_item_t)==224);assert(!strcmp(font_basename("/fonts/name.ttf"),"name.ttf"));
 k_font_dirs[0]=argv[1];k_font_dirs[1]=NULL;user_dir=argv[2];
 const int amounts[]={0,1,24,25,127,128,129,160};
 for(unsigned test=0;test<sizeof(amounts)/sizeof(amounts[0]);++test){
  clear(user_dir);int n=amounts[test];
  for(int i=0;i<n;++i){char name[40];snprintf(name,sizeof(name),"font-%03d%s",i,i%2?".TTF":".ttf");put(user_dir,name);}
  int expected=n>128?128:n;assert(ttf_font_scan()==expected&&ttf_font_count()==expected);
  for(int i=0;i<expected;++i){const ttf_font_item_t *p=ttf_font_item(i);assert(p&&strstr(p->path,user_dir));
   if(i)assert(strcasecmp(ttf_font_item(i-1)->name,p->name)<0);}
  assert(!ttf_font_item(-1)&&!ttf_font_item(expected));
  if(n==128)for(int i=0;i<n;++i){char name[40];snprintf(name,sizeof(name),"font-%03d",i);assert(!strcmp(ttf_font_item(i)->name,name));}
 }
 clear(user_dir);put(k_font_dirs[0],"Same.ttf");put(user_dir,"same.TTF");put(user_dir,"Other.ttf");
 put(user_dir,"._Ignored.ttf");put(user_dir,".hidden.ttf");put(user_dir,"readme.txt");
 #if TEST_METALIO
 assert(ttf_font_scan()==5);
 assert(strcmp(ttf_font_item(3)->path,ttf_font_item(4)->path));
#else
 assert(ttf_font_scan()==2);
 assert(!strcmp(ttf_font_item(0)->name,"Other")&&!strcmp(ttf_font_item(1)->name,"Same"));
 assert(strstr(ttf_font_item(1)->path,k_font_dirs[0]));
#endif
 clear(k_font_dirs[0]);clear(user_dir);
 for(int i=0;i<65;++i){char name[40];snprintf(name,sizeof(name),"asset-%03d.ttf",i);put(k_font_dirs[0],name);}
 for(int i=0;i<63;++i){char name[40];snprintf(name,sizeof(name),"user-%03d.ttf",i);put(user_dir,name);}
 assert(ttf_font_scan()==128&&!strcmp(ttf_font_item(127)->name,"user-062"));
 // Full first directory must not even open the second, or read one entry past capacity.
 clear(k_font_dirs[0]);clear(user_dir);
 for(int i=0;i<128;++i){char name[40];snprintf(name,sizeof(name),"asset-%03d.ttf",i);put(k_font_dirs[0],name);}
 opens=reads=closes=0;assert(ttf_font_scan()==128);assert(opens==1&&closes==1);
 unsigned full_reads=reads;assert(full_reads>=128&&full_reads<=130);
 // Identical roots are visited once; each later scan still reflects filesystem changes.
 clear(k_font_dirs[0]);user_dir=k_font_dirs[0];put(user_dir,"Fresh.ttf");
 opens=reads=closes=0;assert(ttf_font_scan()==1);assert(opens==1&&closes==1);
 unsigned one_reads=reads;assert(one_reads>=2&&one_reads<=4);
 put(user_dir,"Added.ttf");assert(ttf_font_scan()==2);
 clear(user_dir);put(user_dir,"Replacement.ttf");assert(ttf_font_scan()==1);
 assert(!strcmp(ttf_font_item(0)->name,"Replacement"));
 clear(user_dir);user_dir=argv[2];put(user_dir,"ChangedRoot.ttf");
 assert(ttf_font_scan()==1&&!strcmp(ttf_font_item(0)->name,"ChangedRoot"));
 // Missing root is retried on the next scan, not cached as empty.
 clear(user_dir);assert(!rmdir(user_dir));opens=closes=0;assert(ttf_font_scan()==0);
 assert(opens==2&&closes==1);assert(!mkdir(user_dir,0700));put(user_dir,"Remounted.ttf");
 assert(ttf_font_scan()==1&&!strcmp(ttf_font_item(0)->name,"Remounted"));
 // Different long filenames sharing a truncated display label must both remain selectable.
 clear(user_dir);char long_a[80],long_b[80];
 memset(long_a,'a',65);strcpy(long_a+65,"A.ttf");
 memset(long_b,'a',65);strcpy(long_b+65,"B.ttf");
 put(k_font_dirs[0],long_a);put(user_dir,long_a);put(user_dir,long_b);
 assert(ttf_font_scan()==2);
 assert(strcmp(ttf_font_item(0)->name,ttf_font_item(1)->name));
 assert(strcmp(ttf_font_item(0)->path,ttf_font_item(1)->path));
 assert(!strcmp(font_basename(ttf_font_item(0)->path),long_a)||!strcmp(font_basename(ttf_font_item(1)->path),long_a));
 assert(!strcmp(font_basename(ttf_font_item(0)->path),long_b)||!strcmp(font_basename(ttf_font_item(1)->path),long_b));
 assert(allocations==1&&allocated==28672);
 free(s_catalog);s_catalog=NULL;clear(k_font_dirs[0]);clear(user_dir);
 puts("PASS: UTF-8 name boundaries at capacities 0..80, full-filename dedup, catalogue bounds/sort, bounded I/O, root changes/remount; one28672B PSRAM allocation, no font-face loading");
}
'''
with tempfile.TemporaryDirectory(prefix='font-catalog-') as folder:
    work = Path(folder)
    (work / 'assets').mkdir()
    (work / 'fonts').mkdir()
    (work / 'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n')
    (work / 'epdiy.h').write_text('#pragma once\ntypedef struct{int x,y,width,height;} EpdRect;\nenum EpdFontFlags{EPD_DRAW_ALIGN_LEFT,EPD_DRAW_ALIGN_CENTER,EPD_DRAW_ALIGN_RIGHT};\n')
    (work / 'test.c').write_text(unit)
    subprocess.run(['cc', '-DTEST_METALIO=' + ('1' if 'font_filename_make' in source.read_text() else '0'), '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I' + str(work), '-I' + str(root / 'main/font'),
                    str(work / 'test.c'), '-o', str(work / 'test')], check=True)
    subprocess.run([str(work / 'test'), str(work / 'assets'), str(work / 'fonts')], check=True)
