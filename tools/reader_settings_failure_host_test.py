#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Exercise actual preview rollback, partial saves, retry and font restoration failures."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#define ESP_OK 0
#define TTF_FONT_PATH_MAX 192
#define APP_BOOK_TRACK_MIN -5
#define APP_BOOK_TRACK_COUNT 11
typedef int esp_err_t;
typedef int app_redraw_t;
typedef struct {int unused;} app_ctx_t;
typedef struct {char path[192];} ttf_font_item_t;
enum {APP_REDRAW_NONE,APP_REDRAW_PAGE,READING,SHELF};
typedef struct {int x,y,width,height;} EpdRect;
static const char *s_text="body";static size_t s_text_len=4,s_block_count,s_page=2;
static void *s_blocks;static int s_px=48,s_margin=16,s_line_spacing=2,s_view=READING;
static char s_reader_notice[128],s_message[128],s_font_path[192];static int64_t s_reader_notice_until;
static int indent=2,adjustment,tracking=5,engine_indent=2,engine_adjustment,engine_tracking;
static int engine_line,engine_para,builds,saves,fail_build_at,fail_setting,opens,fail_open_at;
static bool locked,fail_all_builds,font_fallback;static char font[192]="old.ttf",saved_font[192]="old.ttf";
static int app_settings_book_px(void){return 48;}
static int app_settings_book_margin(void){return 16;}
static int app_settings_book_line_spacing(void){return 2;}
static int app_settings_book_paragraph_spacing(void){return 3;}
static int app_settings_book_tracking(void){return tracking;}
static int app_settings_book_indent(void){return indent;}
static int app_settings_book_indent_adjust(void){return adjustment;}
static void app_settings_set_book_tracking(uint8_t v){if(fail_setting!=1)tracking=v;}
static void app_settings_set_book_indent(uint8_t v){if(fail_setting!=2)indent=v;}
static void app_settings_set_book_indent_adjust(int8_t v){if(fail_setting!=3)adjustment=v;}
static void lock_draw(void){assert(!locked);locked=true;}
static void unlock_draw(void){assert(locked);locked=false;}
static void invalidate_prep(void){}
static void refresh_chapter_lead(void){assert(locked);}
static void book_layout_set_spacing(int l,int p){assert(locked);engine_line=l;engine_para=p;}
static void book_layout_set_typography(int v){assert(locked);engine_tracking=v;}
static void book_layout_set_first_line_indent(unsigned v){assert(locked);engine_indent=v;}
static void book_layout_set_first_line_indent_adjust(int v){assert(locked);engine_adjustment=v;}
static EpdRect body_rect(void){return (EpdRect){0,0,600,900};}
static EpdRect body_rect_for_tracking(int v){(void)v;return body_rect();}
static bool book_layout_build_blocks(const char *t,size_t n,void *b,size_t c,EpdRect r,int px){
 (void)t;(void)n;(void)b;(void)c;(void)r;(void)px;assert(locked);return ++builds!=fail_build_at && !fail_all_builds;}
static size_t book_layout_page_start_offset(size_t p){return p*100;}
static size_t book_layout_page_for_offset(size_t o){return o/100;}
static void copy_text(char*d,size_t n,const char*s){snprintf(d,n,"%s",s);}
static int64_t esp_timer_get_time(void){return 1000;}
static void free_book(void){assert(!locked);s_text=NULL;}
static void save_progress(void){++saves;}
static bool ttf_font_path_is_builtin(const char*p){return !*p;}
static const char *ttf_font_path(void){return font;}
static bool ttf_font_ready(void){return true;}
static int ttf_font_open(const char*p){if(++opens==fail_open_at){if(font_fallback)copy_text(font,sizeof(font),"builtin");return -1;}copy_text(font,sizeof(font),p);return 0;}
static void reader_font_choice_begin(const char*p){(void)p;}
static void reader_font_choice_finish(void){}
static const char *app_settings_font_path(void){return saved_font;}
static void app_settings_set_font_path(const char*p){if(fail_setting!=4)copy_text(saved_font,sizeof(saved_font),p);}
'''
for name in ('reader_restore_saved_layout','apply_reader_typography','apply_reader_indent','select_reading_font_item'):
 unit+=function(name,ROOT/'main/apps/app_book.c')+'\n'
unit+=r'''
int main(void){
 app_ctx_t ctx={0};
 fail_setting=1;assert(apply_reader_typography(&ctx,7)==APP_REDRAW_PAGE);
 assert(tracking==5&&engine_tracking==0&&builds==2&&!saves&&s_page==2&&strstr(s_reader_notice,"保存失败"));
 fail_setting=0;assert(apply_reader_typography(&ctx,7)==APP_REDRAW_PAGE&&tracking==7&&engine_tracking==2&&saves==1);
 fail_setting=3;assert(apply_reader_indent(&ctx,3,20)==APP_REDRAW_PAGE);
 assert(indent==3&&adjustment==0&&engine_indent==3&&engine_adjustment==0&&saves==1);
 fail_setting=0;assert(apply_reader_indent(&ctx,3,20)==APP_REDRAW_PAGE&&engine_adjustment==20&&saves==2);
 fail_setting=2;assert(apply_reader_indent(&ctx,1,30)==APP_REDRAW_PAGE);
 assert(indent==3&&adjustment==30&&engine_indent==3&&engine_adjustment==30&&saves==2);
 ttf_font_item_t item={"new.ttf"};fail_setting=4;
 assert(select_reading_font_item(&ctx,&item)==APP_REDRAW_PAGE);
 assert(!strcmp(font,"old.ttf")&&!strcmp(saved_font,"old.ttf")&&saves==2&&s_view==READING);
 fail_setting=0;assert(select_reading_font_item(&ctx,&item)==APP_REDRAW_PAGE&&saves==3&&!strcmp(saved_font,"new.ttf"));
 copy_text(item.path,sizeof(item.path),"third.ttf");fail_setting=4;fail_open_at=opens+2;
 assert(select_reading_font_item(&ctx,&item)==APP_REDRAW_PAGE&&s_view==SHELF&&!s_text&&strstr(s_message,"字体恢复失败"));
 s_text="body";s_view=READING;fail_setting=1;fail_build_at=builds+2;
 assert(apply_reader_typography(&ctx,8)==APP_REDRAW_PAGE&&s_view==SHELF&&!s_text&&!locked);
 assert(engine_line==2&&engine_para==3);
 // Two consecutive reflow failures cannot leave a freed/empty layout in READING.
 for(int kind=0;kind<2;++kind){
  s_text="body";s_view=READING;fail_setting=0;fail_all_builds=true;int before=saves;
  app_redraw_t result=kind?apply_reader_indent(&ctx,1,10):apply_reader_typography(&ctx,8);
  assert(result==APP_REDRAW_PAGE&&s_view==SHELF&&!s_text&&!locked&&saves==before);
 }
 fail_all_builds=false;
 // Font load failure: keep the old layout only when the old face survived.
 for(int fallback=0;fallback<2;++fallback){
  s_text="body";s_view=READING;copy_text(font,sizeof(font),"old.ttf");
  fail_open_at=opens+1;font_fallback=fallback;
  assert(select_reading_font_item(&ctx,&item)==APP_REDRAW_PAGE);
  assert(fallback?(s_view==SHELF&&!s_text):(s_view==READING&&s_text));
 }
 font_fallback=false;
 // Initial reflow fails; either the old face or its rebuild also fails.
 for(int mode=0;mode<3;++mode){
  s_text="body";s_view=READING;s_page=2;copy_text(font,sizeof(font),"old.ttf");
  fail_build_at=builds+1;fail_all_builds=mode==1;fail_open_at=mode==2?opens+2:0;
  assert(select_reading_font_item(&ctx,&item)==APP_REDRAW_PAGE);
  assert(mode?(s_view==SHELF&&!s_text):(s_view==READING&&s_text&&s_page==2&&!strcmp(font,"old.ttf")));
 }
 puts("PASS: actual settings rollback, partial multi-key writes, retry, source anchor, font save/restore failure and rebuild OOM safe exit");
}
'''
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined',str(d/'test.c'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
