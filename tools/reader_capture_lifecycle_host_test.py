#!/usr/bin/env python3
"""Exercise actual sampler ownership/cancellation with deterministic FreeRTOS/I2C shims."""
import pathlib, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[1]
common = r"""
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef void *cst836u_handle_t;
#define CST836U_MAX_POINTS 2
typedef struct { bool touched; unsigned count; int x,y; struct { int x,y; } points[2]; } cst836u_touch_t;
typedef int esp_err_t;
#define ESP_OK 0
typedef void *SemaphoreHandle_t;
typedef void *TaskHandle_t;
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(x) (x)
#define ESP_LOGW(...) ((void)0)
static int notifications, reads;
static bool physical_down;
static SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void*)1;}
static int xSemaphoreTake(void *p,unsigned t){(void)p;(void)t;return 1;}
static int xSemaphoreGive(void *p){(void)p;return 1;}
static int xTaskCreate(void(*f)(void*),const char*n,int s,void*a,int p,TaskHandle_t*out){(void)f;(void)n;(void)s;(void)a;(void)p;*out=(void*)1;return pdPASS;}
static void xTaskNotifyGive(void*p){(void)p;++notifications;}
static void ulTaskNotifyTake(int a,unsigned b){(void)a;(void)b;}
static void vTaskDelay(unsigned a){(void)a;}
static int64_t esp_timer_get_time(void){return 1000000;}
static bool cst836u_irq_pending(void*p){(void)p;return false;}
static int cst836u_read(void*p,cst836u_touch_t*out){(void)p;++reads;*out=(cst836u_touch_t){.touched=physical_down};return ESP_OK;}
"""
case = r"""
#include <assert.h>
#include <stdio.h>
#include "main/app/reader_capture.c"
int main(void){
 cst836u_touch_t t={0};int64_t at=0;bool cancel=false;
 reader_capture_begin((void*)1,&t);assert(active&&notifications==1);
 reader_capture_end();assert(!active);
 for(unsigned i=0;i<=READER_CAPTURE_CAPACITY;++i)
   if(!reader_capture_push(&queue,(reader_capture_sample_t){.touch={.touched=true},.at_ms=i}))drain=true;
 assert(reader_capture_take(&t,&at,&cancel)&&cancel&&!t.touched&&drain);
 // Beginning another scan cannot enqueue stale contacts during cancellation drain.
 reader_capture_begin((void*)1,&t);assert(!active&&notifications==1);
 physical_down=true;cancel=false;
 assert(reader_capture_take(&t,&at,&cancel)&&cancel&&drain);
 physical_down=false;
 assert(reader_capture_take(&t,&at,&cancel)&&cancel&&!drain);
 assert(!reader_capture_pending()&&!reader_capture_take(&t,&at,&cancel));
 reader_capture_begin((void*)1,&t);assert(active&&notifications==2);
 reader_capture_clear();assert(!active&&!reader_capture_pending()&&reads==2);
 puts("PASS: actual capture begin/end, overflow cancellation, no stale replay during drain, release recovery and lifecycle reset");
}
"""
with tempfile.TemporaryDirectory() as tmp:
 d=pathlib.Path(tmp);(d/'common.h').write_text(common)
 for name in ['cst836u.h','esp_timer.h','esp_log.h','freertos/FreeRTOS.h','freertos/task.h','freertos/semphr.h']:
  p=d/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "common.h"\n')
 (d/'test.c').write_text(case)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-DESP_PLATFORM','-fsanitize=address,undefined','-I'+str(d),'-I'+str(root),str(d/'test.c'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
