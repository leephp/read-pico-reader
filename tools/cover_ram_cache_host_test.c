/* SPDX-License-Identifier: Apache-2.0
 * 中文：执行共享缓存，覆盖淘汰、版本、容量、内存失败与并发复制。
 * English: Exercise production cache eviction, versions, capacity, OOM and concurrent copies.
 */
#include "cover_ram_cache.h"
#include "../main/kiiko_board_config.h"
#include "esp_heap_caps.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t available=16u*1024u*1024u;
static bool oom;
void *heap_caps_malloc(size_t n,unsigned caps) {assert(caps==3);return oom?NULL:malloc(n);}
void *heap_caps_calloc(size_t n,size_t size,unsigned caps) {assert(caps==3);return oom?NULL:calloc(n,size);}
size_t heap_caps_get_free_size(unsigned caps) {assert(caps==3);return available;}
size_t heap_caps_get_largest_free_block(unsigned caps) {assert(caps==3);return available;}
static cover_ram_key_t key(unsigned n) {return (cover_ram_key_t){{n+1,77,123,456,0,1,0,0}};}
static void *parallel(void *arg) {
    unsigned id=(unsigned)(uintptr_t)arg;
    uint8_t input[64],out[64];memset(input,(int)id,sizeof(input));
    cover_ram_key_t k=key(100+id);
    for(int i=0;i<1000;++i) {
        assert(cover_ram_cache_put(&k,input,sizeof(input),(cover_ram_meta_t){8,8,false}));
        assert(cover_ram_cache_get(&k,out,sizeof(out),NULL));
        assert(!memcmp(input,out,sizeof(out)));
    }
    return NULL;
}
int main(void) {
    enum {CHUNK=512*1024, BUDGET=KIIKO_BOARD_COVER_RAM_BUDGET_KB*1024, COUNT=BUDGET/CHUNK};
    uint8_t *pixels=malloc(CHUNK),*out=malloc(CHUNK);assert(pixels&&out);
    memset(pixels,0x5a,CHUNK);
    cover_ram_meta_t meta={176,240,true},got;
    cover_ram_key_t k=key(0);
    oom=true;assert(!cover_ram_cache_put(&k,pixels,CHUNK,meta));oom=false;
    assert(!cover_ram_cache_get(&k,out,CHUNK,&got));
    for(unsigned i=0;i<COUNT;++i){k=key(i);assert(cover_ram_cache_put(&k,pixels,CHUNK,meta));}
    assert(cover_ram_cache_stats().bytes==BUDGET);
    k=key(0);assert(cover_ram_cache_get(&k,out,CHUNK,&got)&&got.width==176&&got.height==240&&got.white_edge);
    assert(!memcmp(pixels,out,CHUNK));out[0]=0;assert(cover_ram_cache_get(&k,out,CHUNK,NULL)&&out[0]==0x5a);
    k=key(8);assert(cover_ram_cache_put(&k,pixels,CHUNK,meta));
    k=key(1);assert(!cover_ram_cache_get(&k,out,CHUNK,NULL));
    k=key(0);assert(cover_ram_cache_get(&k,out,CHUNK,NULL));
    assert(!cover_ram_cache_get(&k,out,CHUNK-1,NULL));
    cover_ram_cache_clear();assert(!cover_ram_cache_stats().bytes);
    k=key(6);assert(cover_ram_cache_put(&k,pixels,64,meta));
    cover_ram_key_t style=k;style.field[4]=2;style.field[6]=91;
    assert(cover_ram_cache_put(&style,pixels,64,meta));
    cover_ram_key_t revised=k;revised.field[2]++;
    assert(cover_ram_cache_put(&revised,pixels,64,meta));
    assert(!cover_ram_cache_get(&k,out,CHUNK,NULL)&&!cover_ram_cache_get(&style,out,CHUNK,NULL));
    assert(cover_ram_cache_stats().entries==1);
    cover_ram_cache_clear();k=key(6);
    assert(cover_ram_cache_put(&k,pixels,64,meta));
    style=k;style.field[4]=2;assert(cover_ram_cache_put(&style,pixels,64,meta));
    cover_ram_key_t finished=k;finished.field[4]=3;
    assert(cover_ram_cache_put(&finished,pixels,16,meta));
    assert(!cover_ram_cache_get(&k,out,CHUNK,NULL) && !cover_ram_cache_get(&style,out,CHUNK,NULL));
    assert(cover_ram_cache_get(&finished,out,CHUNK,NULL));
    assert(cover_ram_cache_stats().entries==1 && cover_ram_cache_stats().bytes==16);
    available=KIIKO_BOARD_CACHE_PSRAM_RESERVE_KB*1024u+63;
    assert(!cover_ram_cache_put(&k,pixels,64,meta));assert(!cover_ram_cache_stats().bytes);
    available=16u*1024u*1024u;
    assert(cover_ram_cache_put(&k,pixels,CHUNK,meta));
    cover_ram_cache_trim(128*1024);assert(!cover_ram_cache_stats().bytes);
    assert(cover_ram_cache_put(&k,pixels,64,meta));oom=true;
    style=key(7);assert(!cover_ram_cache_put(&style,pixels,64,meta));oom=false;
    assert(!cover_ram_cache_stats().bytes);
    pthread_t threads[4];
    for(unsigned i=0;i<4;++i)assert(!pthread_create(&threads[i],NULL,parallel,(void *)(uintptr_t)i));
    for(unsigned i=0;i<4;++i)assert(!pthread_join(threads[i],NULL));
    assert(cover_ram_cache_stats().entries==4);
    cover_ram_cache_clear();free(pixels);free(out);
    uint8_t source[7*19],target[7*19];for(unsigned i=0;i<sizeof(source);++i)source[i]=(uint8_t)i;
    k=key(80);assert(cover_ram_cache_put_rows(&k,source,19,11,7));memset(target,0xa5,sizeof(target));
    assert(cover_ram_cache_get_rows(&k,target,19,11,7));
    for(unsigned y=0;y<7;++y)for(unsigned x=0;x<19;++x)assert(target[y*19+x]==(x<11?source[y*19+x]:0xa5));
    assert(!cover_ram_cache_get_rows(&k,target,19,12,7));
    assert(!cover_ram_cache_get_rows(&k,target,10,11,7));
    assert(!cover_ram_cache_put_rows(&k,source,19,SIZE_MAX,7));
    assert(!cover_ram_cache_put_rows(&k,source,SIZE_MAX,1,7));
    cover_ram_cache_trim(0);assert(!cover_ram_cache_get_rows(&k,target,19,11,7));
    cover_ram_cache_clear();
    puts("PASS: Pico cache cap, LRU recency, copy ownership, raster metadata, file-version invalidation, Pico shared reserve, trim/OOM fallback and 4000 concurrent copy cycles");
    return 0;
}
