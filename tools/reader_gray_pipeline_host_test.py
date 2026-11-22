#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Audit real reader LUT packing and leading-hold skipping; no optical model is assumed."""
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
render=ROOT/'components/epdiy/src/render.c'
lut=ROOT/'components/epdiy/src/output_common/lut.c'
# Strip only compiler placement attributes for the shared source extractor.
def extract(name,path):
    text=path.read_text().replace('__attribute__((optimize("O3")))', '')
    text='\n'.join(line.lstrip() for line in text.splitlines())
    return function(name,SimpleNamespace(read_text=lambda: text))
unit=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "display_reader_waveform.h"
#include "e0470_epaper_waveform.h"
#define IRAM_ATTR
int epd_width(void){return 1216;}
int epd_height(void){return 684;}
enum EpdRotation epd_get_rotation(void){return EPD_ROT_INVERTED_PORTRAIT;}
static uint8_t s_present[256],s_first_active[256];
static const uint8_t *s_present_for;
static const EpdWaveformPhases *s_first_active_phases;
'''
for name in ('build_first_active','mark_present_line','leading_skip_frames'):
    unit+=extract(name,render)+'\n'
for name in ('epd_build_1ppB_lut_1k','lookup_pixels_in_VE_LUT'):
    unit+=extract(name,lut)+'\n'
unit+=r'''
static unsigned action(const EpdWaveformPhases *p,int phase,unsigned pair){
 unsigned to=pair>>4,from=pair&15;
 return (p->luts[phase*64+to*4+from/4]>>(6-2*(from%4)))&3;
}
int main(void){
 e0470_waveform_init();
 const EpdWaveform *waves[]={display_reader_soft_turn_waveform(),&E0470_FULL_WAVEFORM,
  display_reader_water_waveform(),display_reader_white_push_waveform()};
 const int modes[]={MODE_GL16,MODE_GC16,MODE_GL16,MODE_GL16};
 const char *names[]={"ordinary","cleanup","ripple","metadata-white"};
 _Alignas(16) uint32_t table[256];
 _Alignas(16) uint8_t target[64]={0},prior[64]={0},diff[128];
 unsigned checked=0;
 for(unsigned w=0;w<4;++w){
  const EpdWaveformPhases *p=e0470_waveform_phases(waves[w],modes[w]);
  assert(p&&p->phases>0&&p->phases<=64&&!p->phase_times);
  uint8_t original[64*64];memcpy(original,p->luts,p->phases*64);
  // Every phase and all 256 old/target pairs use the actual 1K builder and scalar lane lookup.
  for(int f=0;f<p->phases;++f){
   epd_build_1ppB_lut_1k((uint8_t*)table,p,f);
   for(unsigned pair=0;pair<256;++pair){
    assert(table[pair]==action(p,f,pair));
    uint32_t packed=0;unsigned expected=0;
    for(unsigned lane=0;lane<4;++lane){
     unsigned v=(pair+lane*73)&255;packed|=v<<(lane*8);expected|=action(p,f,v)<<(lane*2);
    }
    assert(lookup_pixels_in_VE_LUT(packed,(uint8_t*)table)==expected);++checked;
   }
  }
  // Scan every single pair, then mixed frames. Cached and uncached pair census must agree.
  for(unsigned sample=0;sample<512;++sample){
   bool expected[256]={0};
   for(unsigned x=0;x<128;++x){
    unsigned pair=sample<256?sample:((x*29+sample)&255);
    diff[x]=pair;expected[pair]=true;
    unsigned shift=4*(x&1);
    target[x/2]=(uint8_t)((target[x/2]&~(15u<<shift))|((pair>>4)<<shift));
    prior[x/2]=(uint8_t)((prior[x/2]&~(15u<<shift))|((pair&15)<<shift));
   }
   memset(s_present,0,sizeof(s_present));mark_present_line(target,prior,128);
   for(unsigned pair=0;pair<256;++pair)assert((s_present[pair]!=0)==expected[pair]);
   s_present_for=diff;int cached=leading_skip_frames(p,diff,(EpdRect){0,0,128,1});
   s_present_for=NULL;int scanned=leading_skip_frames(p,diff,(EpdRect){0,0,128,1});
   assert(cached==scanned&&cached>=0&&cached<p->phases);
   for(int f=0;f<cached;++f)for(unsigned pair=0;pair<256;++pair)
    if(expected[pair])assert(action(p,f,pair)==0);
  }
  // Publish pulse inventory as evidence, not a prediction of optical whiteness.
  for(unsigned gray=0;gray<16;++gray){
   unsigned counts[4]={0};
   for(int f=0;f<p->phases;++f)++counts[action(p,f,gray*17)];
   printf("%s phases=%d equal-gray=%u hold=%u black=%u white=%u other=%u\n",
    names[w],p->phases,gray,counts[0],counts[1],counts[2],counts[3]);
  }
  for(unsigned from=0;from<16;++from){
   unsigned counts[4]={0};
   for(int f=0;f<p->phases;++f)++counts[action(p,f,0xf0|from)];
   printf("%s to-white from=%u hold=%u black=%u white=%u other=%u\n",
    names[w],from,counts[0],counts[1],counts[2],counts[3]);
  }
  assert(!memcmp(original,p->luts,p->phases*64));
 }
 printf("PASS: %u phase/pair LUT and lane checks; 2048 cached/scanned pair censuses; no active phase skipped; waveform bytes unchanged\n",checked);
}
'''
with tempfile.TemporaryDirectory(prefix='pico-gray-pipeline-') as tmp:
    c=Path(tmp)/'test.c';c.write_text(unit);exe=Path(tmp)/'test'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unknown-attributes',
        '-fsanitize=address,undefined','-g','-Imain','-Itools/waveform_host_stubs',
        '-Icomponents/epdiy/include','-Icomponents/epdiy/src',
        '-Icomponents/e0470_epaper_waveform/include','-Icomponents/e0470_epaper_waveform/waveforms',
        str(c),'main/display_reader_waveform.c',
        'components/e0470_epaper_waveform/e0470_epaper_waveform.c',
        'components/e0470_epaper_waveform/e0470_waveform_trim.c','-o',str(exe)],cwd=ROOT,check=True)
    subprocess.run([str(exe)],check=True)
