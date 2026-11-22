/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "cst836u.h"
#include <stdint.h>
#ifdef ESP_PLATFORM
void reader_capture_begin(cst836u_handle_t tp, const cst836u_touch_t *seed);
void reader_capture_end(void);
bool reader_capture_take(cst836u_touch_t *touch, int64_t *at_ms, bool *cancel);
bool reader_capture_pending(void);
void reader_capture_clear(void);
#else
static inline void reader_capture_begin(cst836u_handle_t tp,const cst836u_touch_t *seed){(void)tp;(void)seed;}
static inline void reader_capture_end(void){}
static inline bool reader_capture_take(cst836u_touch_t *t,int64_t *at,bool *cancel){(void)t;(void)at;(void)cancel;return false;}
static inline bool reader_capture_pending(void){return false;}
static inline void reader_capture_clear(void){}
#endif
