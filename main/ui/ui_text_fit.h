// SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
#pragma once
#include <string.h>

// 保持字号，省略中间完整字符；保留名称首尾，无分配且不增长缓冲。
// Keep the font size and both ends of a name; remove whole UTF-8 characters without allocation or growth.
static inline void ui_text_fit_middle(char *text,int px,int width,int (*measure)(int,const char *)) {
    if(!*text || measure(px,text)<=width)return;
    size_t n=strlen(text);
    if(width<=0){text[0]=0;return;}
    if(n<3){
        // A three-byte ellipsis cannot fit in a one/two-byte label's buffer.
        while(n && measure(px,text)>width){
            --n;while(n&&((unsigned char)text[n]&0xc0)==0x80)--n;
            text[n]=0;
        }
        return;
    }
    size_t left=n/2;
    while(left && ((unsigned char)text[left]&0xc0)==0x80)--left;
    size_t right=left;
    while(right<n && right-left<3){++right;while(right<n&&((unsigned char)text[right]&0xc0)==0x80)++right;}
    while(right-left<3 && left){--left;while(left&&((unsigned char)text[left]&0xc0)==0x80)--left;}
    memmove(text+left+3,text+right,n-right+1);memcpy(text+left,"…",3);
    while(measure(px,text)>width){
        n=strlen(text);
        if(!left && n==3){text[0]=0;break;}
        if(left && left>=n-left-3){
            size_t start=left-1;while(start&&((unsigned char)text[start]&0xc0)==0x80)--start;
            memmove(text+start,text+left,n-left+1);left=start;
        }else{
            size_t end=left+4;while(end<n&&((unsigned char)text[end]&0xc0)==0x80)++end;
            memmove(text+left+3,text+end,n-end+1);
        }
    }
}
