/* SPDX-License-Identifier: Apache-2.0
 * Reader scans only: capture touch/key-strip reports without dispatching UI from another task.
 * The mutex joins the last I2C read before the main loop or sleep regains ownership.
 */
#include "reader_capture.h"
#include "reader_capture_queue.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
static SemaphoreHandle_t lock;
static TaskHandle_t worker;
static cst836u_handle_t device;
static bool active, drain, unavailable;
static reader_capture_queue_t queue;
static cst836u_touch_t last;
static int64_t polled;

static bool changed(const cst836u_touch_t *a,const cst836u_touch_t *b) {
    if(a->touched!=b->touched || a->count!=b->count) return true;
    if(!a->touched) return false;
    if(a->x!=b->x || a->y!=b->y) return true;
    for(unsigned i=0;i<a->count && i<CST836U_MAX_POINTS;++i)
        if(a->points[i].x!=b->points[i].x || a->points[i].y!=b->points[i].y) return true;
    return false;
}
static void capture_task(void *unused) {
    (void)unused;
    for(;;) {
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        for(;;) {
            xSemaphoreTake(lock,portMAX_DELAY);
            if(!active){xSemaphoreGive(lock);break;}
            int64_t now=esp_timer_get_time()/1000;
            if(cst836u_irq_pending(device) || now-polled >= (last.touched?100:500)) {
                cst836u_touch_t fresh={0};
                esp_err_t err=cst836u_read(device,&fresh);
                polled=now;
                if(err!=ESP_OK) {queue.overflow=true;queue.count=0;drain=true;}
                else if(changed(&last,&fresh)) {
                    if(!reader_capture_push(&queue,(reader_capture_sample_t){fresh,now})) drain=true;
                    last=fresh;
                }
            }
            xSemaphoreGive(lock);
            vTaskDelay(pdMS_TO_TICKS(5)>0?pdMS_TO_TICKS(5):1);
        }
    }
}
void reader_capture_begin(cst836u_handle_t tp,const cst836u_touch_t *seed) {
    if(!tp || !seed || unavailable)return;
    if(!lock)lock=xSemaphoreCreateMutex();
    if(!lock){unavailable=true;ESP_LOGW("reader-input","no capture mutex; using synchronous input");return;}
    if(!worker && xTaskCreate(capture_task,"reader-input",4096,NULL,2,&worker)!=pdPASS){
        worker=NULL;unavailable=true;ESP_LOGW("reader-input","no capture task; using synchronous input");return;
    }
    xSemaphoreTake(lock,portMAX_DELAY);
    // A cancelled contact must be drained by the main loop before sampling resumes.
    if(drain){xSemaphoreGive(lock);return;}
    device=tp;
    // Do not rewind physical state to an older queued sample during consecutive scans.
    if(!queue.count && !queue.overflow)last=*seed;
    active=true;polled=0;
    xSemaphoreGive(lock);
    xTaskNotifyGive(worker);
}
void reader_capture_end(void) {
    if(!lock)return;
    xSemaphoreTake(lock,portMAX_DELAY);active=false;xSemaphoreGive(lock);
}
bool reader_capture_pending(void) {
    if(!lock)return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool pending=queue.count || queue.overflow || drain;
    xSemaphoreGive(lock);return pending;
}
bool reader_capture_take(cst836u_touch_t *touch,int64_t *at_ms,bool *cancel) {
    if(!lock)return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool found=false;
    if(queue.overflow) {
        queue=(reader_capture_queue_t){0};*cancel=true;
        *touch=(cst836u_touch_t){0};*at_ms=esp_timer_get_time()/1000;found=true;
        ESP_LOGW("reader-input","capture overflow/read failure; cancel contact until release");
    } else if(drain) {
        cst836u_touch_t fresh={0};
        if(cst836u_read(device,&fresh)==ESP_OK && !fresh.touched)drain=false;
        *cancel=true;*touch=(cst836u_touch_t){0};*at_ms=esp_timer_get_time()/1000;found=true;
    } else {
        reader_capture_sample_t sample;
        found=reader_capture_pop(&queue,&sample);
        if(found){*touch=sample.touch;*at_ms=sample.at_ms;}
    }
    xSemaphoreGive(lock);return found;
}
void reader_capture_clear(void) {
    reader_capture_end();
    if(!lock)return;
    xSemaphoreTake(lock,portMAX_DELAY);queue=(reader_capture_queue_t){0};drain=false;xSemaphoreGive(lock);
}
