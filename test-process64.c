#include <assert.h>
#include <stdio.h>
#include <string.h>
#define NTPERF_TEST64
#include "process.h"
#include "process-win64.h"
static void put(unsigned char *b, unsigned int at, NP_U32 n)
{
    b[at]=(unsigned char)n; b[at+1]=(unsigned char)(n>>8);
    b[at+2]=(unsigned char)(n>>16); b[at+3]=(unsigned char)(n>>24);
}
static void wide(unsigned char *b, unsigned int at, NP_SIZE n)
{
    put(b,at,(NP_U32)n); put(b,at+4,(NP_U32)(n>>32));
}
int main(void)
{
    unsigned char b[576];
    PROCDATA p, old;
    NP_U32 next;
    NP_SIZE base, bytes;
    char number[24];
    CPUINFO c[1];
    int busy,kernel;
    base=((NP_SIZE)0x1234<<32)+0x10000;
    bytes=(NP_SIZE)5*1024*1024*1024*1024; /* 5 TiB; KiB also exceeds 32 bits. */
    memset(b,0,sizeof(b));
    put(b,0,0x100); put(b,4,9); put(b,0x20,1234);
    put(b,0x28,20000000); put(b,0x30,10000000);
    put(b,0x38,8|(8U<<16)); wide(b,0x40,base+0x210);
    wide(b,0x50,27); wide(b,0x78,bytes); wide(b,0x90,(NP_SIZE)8*1024*1024*1024);
    put(b,0x48,8); put(b,0x60,321); put(b,0x80,77); wide(b,0x88,(NP_SIZE)16*1024*1024*1024);
    wide(b,0xa0,4096); wide(b,0xb0,2048); wide(b,0xc8,12288);
    assert(decodeProcess(b,sizeof(b),base,0,&p,&next));
    assert(next==0x100 && p.pid==27 && p.threads==9);
    assert(p.nameOffset==0x210 && p.nameBytes==8);
    assert(p.virtualKB==bytes/1024 && p.workingKB==8388608);
    assert(p.priority==8 && p.handles==321 && p.faults==77);
    assert(p.peakKB==16777216 && p.pagedKB==4 && p.nonpagedKB==2 && p.commitKB==12);
    formatSize(number,p.virtualKB); assert(!strcmp(number,"5368709120"));
    formatSize(number,(NP_SIZE)-1); assert(!strcmp(number,"18446744073709551615"));
    formatSize(number,0); assert(!strcmp(number,"0"));
    assert(processTime(&p)==30000000.0);
    old=p; assert(sameProcess(&p,&old)); ++p.created.low; assert(!sameProcess(&p,&old));
    assert(decodeProcess(b,sizeof(b),base,0x100,&p,&next) && next==0);
    assert(!decodeProcess(b,255,base,0,&p,&next));
    put(b,0,0x108+1); assert(!decodeProcess(b,sizeof(b),base,0,&p,&next));
    put(b,0,0x100);
    wide(b,0x40,base-2); assert(!decodeProcess(b,sizeof(b),base,0,&p,&next));
    wide(b,0x40,base+sizeof(b)-2); assert(!decodeProcess(b,sizeof(b),base,0,&p,&next));
    wide(b,0x40,base+0x210+1); assert(!decodeProcess(b,sizeof(b),base,0,&p,&next));
    wide(b,0x40,base+0x210+((NP_SIZE)1<<32));
    assert(!decodeProcess(b,sizeof(b),base,0,&p,&next));
    wide(b,0x40,base+0x210); put(b,0x38,7|(8U<<16));
    assert(!decodeProcess(b,sizeof(b),base,0,&p,&next));
    put(b,0x38,8|(6U<<16)); assert(!decodeProcess(b,sizeof(b),base,0,&p,&next));
    put(b,0x38,0); wide(b,0x40,0);
    assert(decodeProcess(b,sizeof(b),base,0,&p,&next));
    wide(b,0x50,(NP_SIZE)1<<32); assert(!decodeProcess(b,sizeof(b),base,0,&p,&next));
    memset(c,0,sizeof(c)); cpuPercentages(c,c,1,&busy,&kernel);
    assert(busy==-1 && kernel==-1);
    puts("PASS: x64 record chain, high pointers, 5 TiB VM, 64-bit formatting, malformed records, PID identity");
    return 0;
}
