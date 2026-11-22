/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "reader_capture_queue.h"
int main(void){
    reader_capture_queue_t q={0};reader_capture_sample_t got;
    for(int round=0;round<4;++round){
        for(unsigned i=0;i<READER_CAPTURE_CAPACITY;++i){
            reader_capture_sample_t s={.touch={.touched=(i%2)==0,.count=(i%2)==0,.x=(i%4<2?80:600),.y=1100},.at_ms=round*1000+i*10};
            assert(reader_capture_push(&q,s));
        }
        for(unsigned i=0;i<READER_CAPTURE_CAPACITY;++i){
            assert(reader_capture_pop(&q,&got));assert(got.at_ms==round*1000+i*10);
            assert(got.touch.touched==((i%2)==0));assert(got.touch.x==(i%4<2?80:600));
        }
        assert(!reader_capture_pop(&q,&got));
    }
    for(unsigned i=0;i<READER_CAPTURE_CAPACITY;++i)assert(reader_capture_push(&q,(reader_capture_sample_t){0}));
    assert(!reader_capture_push(&q,(reader_capture_sample_t){0}));
    assert(q.overflow && !reader_capture_pop(&q,&got));
    assert(!reader_capture_push(&q,(reader_capture_sample_t){0}));
    puts("PASS: ordered touch/key-strip press/release, timestamps, ring reuse and fail-closed overflow");
}
