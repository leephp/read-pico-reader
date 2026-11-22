/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "cst836u.h"
#include <stdint.h>
#include <string.h>

#define READER_CAPTURE_CAPACITY 64
typedef struct { cst836u_touch_t touch; int64_t at_ms; } reader_capture_sample_t;
typedef struct {
    reader_capture_sample_t samples[READER_CAPTURE_CAPACITY];
    unsigned head, count;
    bool overflow;
} reader_capture_queue_t;
// Fail closed on overflow: never replay a press whose release was lost.
static inline bool reader_capture_push(reader_capture_queue_t *q, reader_capture_sample_t sample) {
    if (q->overflow) return false;
    if (q->count == READER_CAPTURE_CAPACITY) {
        q->head=q->count=0; q->overflow=true; return false;
    }
    q->samples[(q->head+q->count)%READER_CAPTURE_CAPACITY]=sample;
    ++q->count; return true;
}
static inline bool reader_capture_pop(reader_capture_queue_t *q, reader_capture_sample_t *sample) {
    if (!q->count) return false;
    *sample=q->samples[q->head]; q->head=(q->head+1)%READER_CAPTURE_CAPACITY;
    --q->count; return true;
}
