#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Bounded UTF-8 middle ellipsis, retaining font size and label ends."""
from pathlib import Path
import subprocess,tempfile,json
root=Path(__file__).resolve().parents[1]
texts=['OK','AB','é','Short','中文书名与结尾','Prefix '+'字体'*20+' Regular','Prefix '+'字体'*20+' Bold','é中𠀀'*20,'a','中','', '长书名'*60]
code='#include "'+str(root/'main/ui/ui_text_fit.h')+'"\n'+r'''
#include <assert.h>
#include <stdio.h>
static int width(int px,const char *s){int n=0;for(;*s;++s)if(((unsigned char)*s&0xc0)!=0x80)n+=px;return n;}
int main(void){
'''
for index,text in enumerate(texts):
    for w in range(0,161,4):
        code+=f'{{char buffer[1024];memset(buffer,0x5a,sizeof(buffer));strcpy(buffer,{json.dumps(text,ensure_ascii=False)});ui_text_fit_middle(buffer,8,{w},width);assert(width(8,buffer)<={w});assert(strlen(buffer)<={len(text.encode())});for(unsigned i={len(text.encode())+1};i<sizeof(buffer);++i)assert(buffer[i]==0x5a);printf("{index} {w} %s\\n",buffer);}}\n'
code+='return 0;}\n'
with tempfile.TemporaryDirectory() as folder:
    p=Path(folder);(p/'test.c').write_text(code)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    result=subprocess.run([str(p/'test')],capture_output=True,check=True).stdout.decode('utf-8')
    for line in result.splitlines():
        index,w,shown=line.split(' ',2);original=texts[int(index)];w=int(w)
        if len(original.encode())<3:assert shown==original[:w//8]
        elif len(original)*8<=w:assert shown==original
        elif shown and len(original.encode())<3:
            assert original.startswith(shown)
        elif shown:
            assert '…' in shown
            head,tail=shown.split('…');assert original.startswith(head) and original.endswith(tail)
print('PASS:',len(texts)*41,'middle-ellipsis/short-label cases, UTF-8, fixed size, retained ends, measured width and buffer sentinels')
