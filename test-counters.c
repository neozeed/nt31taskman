#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "counters.h"
int main(void)
{
    TIME64 before, after;
    CPUINFO old[2], now[2];
    int busy, kernel;
    before.high=9; before.low=0xfffffff0U;
    after.high=10; after.low=0x10;
    assert(timeDelta(after,before)==32.0);
    assert(timeDelta(before,after)<0.0);
    memset(old,0,sizeof(old)); memset(now,0,sizeof(now));
    now[0].idle.low=50; now[0].kernel.low=75; now[0].user.low=25;
    cpuPercentages(now,old,1,&busy,&kernel);
    assert(busy==50 && kernel==25);
    memset(now,0,sizeof(now));
    now[0].idle.low=100; now[0].kernel.low=100;
    now[1].user.low=100;
    cpuPercentages(now,old,2,&busy,&kernel);
    assert(busy==50 && kernel==0);
    now[0].idle.low=0; now[0].kernel.low=0;
    now[1].user.low=0;
    cpuPercentages(now,old,2,&busy,&kernel);
    assert(busy==-1 && kernel==-1);
    old[0].kernel.low=1;
    cpuPercentages(now,old,2,&busy,&kernel);
    assert(busy==-1 && kernel==-1);
    assert(percentage(200.0,100.0)==100);
    assert(percentage(0.0,0.0)==-1);
    puts("PASS: 64-bit rollover/reset, busy vs idle/kernel, SMP normalization, zero delta, clamp");
    return 0;
}
