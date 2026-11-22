#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compile the real PMIC driver; trace shutdown register writes and pin ordering."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = Path(os.environ.get('SY7636A_TEST_SOURCE', ROOT / 'components/sy7636a/sy7636a.c'))
header = ROOT / 'components/sy7636a/include/sy7636a.h'

def strip_includes(text):
    return re.sub(r'^\s*#(?:include\b[^\n]*|pragma once)\s*$', '', text, flags=re.M)

unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int esp_err_t;
typedef int gpio_num_t;
typedef void *i2c_master_dev_handle_t;
typedef void *i2c_master_bus_handle_t;
typedef struct {uint64_t pin_bit_mask; int mode,pull_up_en,pull_down_en,intr_type;} gpio_config_t;
typedef struct {int dev_addr_length,device_address,scl_speed_hz;} i2c_device_config_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_NO_MEM -3
#define ESP_ERR_TIMEOUT -4
#define GPIO_NUM_NC -1
#define GPIO_IS_VALID_GPIO(n) ((n)>=0)
#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_INPUT 2
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
#define I2C_ADDR_BIT_LEN_7 7
#define pdMS_TO_TICKS(n) (n)
static unsigned success_logs,failed_logs;
static void log_ignore(const char *tag,const char *fmt,...){(void)tag;
    if(!strcmp(fmt,"rails off"))++success_logs;
    if(strstr(fmt,"rails off unconfirmed"))++failed_logs;
}
static const char *esp_err_to_name(int e){(void)e;return "error";}
#define ESP_LOGE(...) log_ignore(__VA_ARGS__)
#define ESP_LOGI(...) log_ignore(__VA_ARGS__)
static int64_t now;
static bool en,vcom,pgood,fail_vcom;
static unsigned fail_off;
static unsigned step,off_step,vcom_off_step,en_off_step,off_writes;
static int64_t off_time,en_off_time;
static uint8_t regs[9],read_address,last_off;
static void vTaskDelay(unsigned ms){now+=(int64_t)ms*1000;}
static void esp_rom_delay_us(unsigned us){now+=us;}
static int64_t esp_timer_get_time(void){return now;}
static int gpio_config(const gpio_config_t *c){(void)c;return 0;}
static int gpio_set_level(int p,int v){(void)p;(void)v;return 0;}
static int gpio_get_level(int p){(void)p;return pgood;}
static int i2c_master_bus_add_device(void *b,const i2c_device_config_t *c,void **d){assert(b&&c);*d=(void*)1;return 0;}
static int i2c_master_bus_rm_device(void *d){assert(d);return 0;}
static int i2c_master_transmit(void *d,const uint8_t *p,size_t n,int timeout){
    assert(d&&en&&timeout==100&&(n==1||n==2));
    assert(p[0]<sizeof(regs));++step;
    if(n==2&&p[0]==0&&!(p[1]&0x80)&&(fail_off&2))return ESP_FAIL;
    if(n==1)read_address=p[0];
    else {
        regs[p[0]]=p[1];
        if(p[0]==0&&!(p[1]&0x80)){
            last_off=p[1];off_step=step;off_time=now;++off_writes;
        }
    }
    return 0;
}
static int i2c_master_receive(void *d,uint8_t *p,size_t n,int timeout){
    assert(d&&en&&n==1&&timeout==100);*p=regs[read_address];return 0;
}
static int enable(bool on){
    if(!on&&(fail_off&4))return ESP_ERR_TIMEOUT;
    ++step;en=on;
    if(!on){en_off_step=step;en_off_time=now;memset(regs,0,sizeof(regs));}
    return 0;
}
static int vcom_enable(bool on){
    if(!on&&(fail_off&1))return ESP_FAIL;
    ++step;if(on&&fail_vcom)return ESP_FAIL;
    vcom=on;if(!on)vcom_off_step=step;return 0;
}
static int power_good(void){return pgood;}
'''
unit += strip_includes(header.read_text()) + '\n' + strip_includes(source.read_text())
unit += r'''
static void reset(void){
    now=0;en=vcom=fail_vcom=false;pgood=true;
    fail_off=0;
    step=off_step=vcom_off_step=en_off_step=off_writes=0;
    memset(regs,0,sizeof(regs));last_off=0xff;
}
int main(void){
    unsigned cases=0;
    for(unsigned manual=0;manual<2;++manual)for(unsigned bits=0;bits<32;++bits){
        if(bits&~0x17U)continue;
        for(unsigned scenario=0;scenario<3;++scenario){
            reset();sy7636a_config_t cfg=SY7636A_CONFIG_DEFAULT();
            cfg.en_fn=enable;cfg.vcom_en_fn=vcom_enable;cfg.pgood_fn=power_good;
            cfg.power.vcom_manual=manual;cfg.power.discharge=bits;cfg.power.vcom_mv=1500;
            sy7636a_handle_t h=NULL;
            assert(sy7636a_init((void*)1,&cfg,&h)==ESP_OK);
            pgood=scenario!=1;fail_vcom=scenario==2;
            int result=sy7636a_power_on(h);
            if(!scenario){
                assert(result==ESP_OK&&en&&vcom&&sy7636a_rails_on(h));
                assert(regs[0]==(0x80U|(manual?0x40U:0)|bits));
                assert(regs[1]==150&&regs[3]==0x66&&regs[6]==0xaa);
                // Scanner has already gated VCOM off; shutdown must retain that mode.
                assert(vcom_enable(false)==ESP_OK);
                assert(sy7636a_power_off(h)==ESP_OK);
                assert(en_off_time-off_time==500000);
            }else{
                assert(result==(scenario==1?ESP_ERR_TIMEOUT:ESP_FAIL));
                if(scenario==1)assert(en_off_time-off_time==150000);
            }
            assert(off_writes==1);
            assert(last_off==((manual?0x40U:0)|bits));
            assert(vcom_off_step<off_step&&off_step<en_off_step);
            assert(!en&&!vcom&&!sy7636a_rails_on(h));
            assert(sy7636a_get_config(h)->vcom_mv==1500);
            assert(sy7636a_get_config(h)->discharge==bits);
            // EN reset must not lose cached settings on the next successful cycle.
            pgood=true;fail_vcom=false;
            assert(sy7636a_power_on(h)==ESP_OK);
            assert(regs[0]==(0x80U|(manual?0x40U:0)|bits));
            assert(sy7636a_power_off(h)==ESP_OK);
            assert(sy7636a_deinit(h)==ESP_OK);++cases;
        }
    }
    assert(sy7636a_power_off(NULL)==ESP_ERR_INVALID_ARG);
    for(unsigned faults=1;faults<8;++faults){
        reset();sy7636a_config_t cfg=SY7636A_CONFIG_DEFAULT();
        cfg.en_fn=enable;cfg.vcom_en_fn=vcom_enable;cfg.pgood_fn=power_good;
        sy7636a_handle_t h=NULL;assert(sy7636a_init((void*)1,&cfg,&h)==ESP_OK);
        assert(sy7636a_power_on(h)==ESP_OK);fail_off=faults;
        unsigned logged=success_logs;
        assert(sy7636a_power_off(h)!=ESP_OK);
        assert(success_logs==logged&&failed_logs);
        assert(sy7636a_rails_on(h)); // Unconfirmed off remains pending, even if fallback EN worked.
        if(faults&4)assert(en);else assert(!en);
        // Do not claim already-on success or begin a new scan through a failed shutdown.
        if(faults&5)assert(sy7636a_power_on(h)!=ESP_OK);
        else assert(sy7636a_power_on(h)==ESP_OK); // EN fallback reset already recovered the failed OP write.
        fail_off=0;assert(sy7636a_power_off(h)==ESP_OK);
        assert(!en&&!vcom&&!sy7636a_rails_on(h));
        assert(sy7636a_power_on(h)==ESP_OK&&en&&vcom);
        assert(sy7636a_power_off(h)==ESP_OK);
        assert(sy7636a_deinit(h)==ESP_OK);
    }
    reset();sy7636a_config_t cfg=SY7636A_CONFIG_DEFAULT();
    cfg.en_fn=enable;cfg.vcom_en_fn=vcom_enable;cfg.pgood_fn=power_good;
    sy7636a_handle_t h=NULL;assert(sy7636a_init((void*)1,&cfg,&h)==ESP_OK);
    bool woke=false;assert(sy7636a_i2c_begin(h,&woke)==ESP_OK&&woke);
    fail_off=4;sy7636a_i2c_end(h,woke);assert(en&&sy7636a_rails_on(h));
    assert(sy7636a_deinit(h)!=ESP_OK); // Keep the handle for recovery, do not free an active device.
    fail_off=0;assert(sy7636a_power_on(h)==ESP_OK&&en&&vcom);
    assert(sy7636a_deinit(h)==ESP_OK&&!en);
    printf("PASS: %u actual-driver cases, manual/auto modes, all discharge masks, normal off/PGOOD timeout/VCOM enable failure, ordering, hold time and restart\n",cases);
    puts("PASS: seven shutdown fault combinations, truthful logs, conservative state, retry/restart and failed EN teardown recovery");
}
'''
with tempfile.TemporaryDirectory() as folder:
    c, binary = Path(folder) / 'test.c', Path(folder) / 'test'
    c.write_text(unit)
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
