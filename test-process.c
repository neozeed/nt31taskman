#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "process.h"
#include "process-nt3x.h"
static void put(unsigned char *b, unsigned int at, NP_U32 n)
{
    b[at]=(unsigned char)n; b[at+1]=(unsigned char)(n>>8);
    b[at+2]=(unsigned char)(n>>16); b[at+3]=(unsigned char)(n>>24);
}
int main(void)
{
    unsigned char b[320];
    PROCDATA p,old;
    NP_U32 next;
    char number[24];
    CPUINFO c[1];
    int busy,kernel;
    memset(b,0,sizeof(b));
    put(b,4,3); put(b,0x20,1234); put(b,0x28,20000000);
    put(b,0x30,10000000); put(b,0x44,27);
    put(b,0x4c,0xdeadbeefU); /* NT 3.1 I/O counter: must not become handles. */
    put(b,0x68,8192); put(b,0x5c,65536);
    put(b,0x40,13); put(b,0x60,77); put(b,0x64,16384);
    put(b,0x70,4096); put(b,0x78,2048); put(b,0x84,12288);
    b[0x38]=8; b[0x3a]=8; put(b,0x3c,0x10000+0x88);
    b[0x88]='t'; b[0x8a]='e'; b[0x8c]='s'; b[0x8e]='t';
    assert(decodeProcess(b,sizeof(b),0x10000,0,&p,&next));
    assert(!next && p.pid==27 && p.threads==3);
    assert(p.workingKB==8 && p.virtualKB==64 && p.nameBytes==8 && p.nameOffset==0x88);
    assert(p.priority==13 && p.faults==77 && p.handles==0);
    assert(p.peakKB==16 && p.pagedKB==4 && p.nonpagedKB==2 && p.commitKB==12);
    assert(processTime(&p)==30000000.0);
    formatSize(number,p.virtualKB); assert(!strcmp(number,"64"));
    old=p; assert(sameProcess(&old,&p));
    ++p.created.low; assert(!sameProcess(&old,&p));
    assert(!decodeProcess(b,0x87,0x10000,0,&p,&next));
    put(b,0,4); assert(!decodeProcess(b,sizeof(b),0x10000,0,&p,&next));
    put(b,0,0); put(b,0x3c,0xfffe);
    assert(!decodeProcess(b,sizeof(b),0x10000,0,&p,&next));
    put(b,0x3c,0x10000+318);
    assert(!decodeProcess(b,sizeof(b),0x10000,0,&p,&next));
    put(b,0x3c,0x10000+0x88); b[0x38]=7;
    assert(!decodeProcess(b,sizeof(b),0x10000,0,&p,&next));
    b[0x38]=8; b[0x3a]=6;
    assert(!decodeProcess(b,sizeof(b),0x10000,0,&p,&next));
    b[0x38]=0; b[0x3a]=0; put(b,0x3c,0);
    assert(decodeProcess(b,sizeof(b),0x10000,0,&p,&next));
    memset(c,0,sizeof(c)); cpuPercentages(c,c,1,&busy,&kernel);
    assert(busy==-1 && kernel==-1);
    puts("PASS: NT 3.x offsets, pointer bounds, truncated/invalid records, empty names, PID reuse identity");
    return 0;
}
