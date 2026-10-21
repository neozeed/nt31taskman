#ifdef _WIN64
#error nt3x.c requires the x86 NT 3.x target
#endif
#define NTPERF_BACKEND
#include "backend.h"
#include "process-nt3x.h"
static int backendCpuSupported(void) { return 1; }
static LONG backendReadHandles(PROCDATA *rows, DWORD count);
#include "native-query.inc"
int backendInit(DWORD *version)
{
    *version=GetVersion();
    return initNative();
}
const char *backendName(void) { return "NT 3.x x86"; }
void backendReadMemory(SAMPLE *s, const SYSTEM_INFO *info)
{
    DWORD perf[78], cache[3];
    ULONG returned;
    MEMORYSTATUS memory;
    clearBytes(perf, sizeof(perf));
    returned = 0;
    s->perfStatus = query(2, perf, sizeof(perf), &returned);
    s->memoryValid = s->perfStatus >= 0 && (!returned || returned >= 120);
    if (s->memoryValid) {
        memory.dwLength = sizeof(memory);
        GlobalMemoryStatus(&memory);
        s->totalKB = memory.dwTotalPhys / 1024;
        s->availableKB = perf[11] * (info->dwPageSize / 1024);
        s->commitKB = perf[12] * (info->dwPageSize / 1024);
        s->limitKB = perf[13] * (info->dwPageSize / 1024);
        s->peakKB = perf[14] * (info->dwPageSize / 1024);
        s->pagedKB = perf[28] * (info->dwPageSize / 1024);
        s->nonpagedKB = perf[29] * (info->dwPageSize / 1024);
    }

    /* File cache (class 21) may be missing on 3.1; the UI then shows Used. */
    s->cacheValid=0;
    if (!s->memoryValid || query(21,cache,sizeof(cache),NULL)<0 || cache[0]/1024>s->totalKB) return;
    s->cacheKB=cache[0]/1024; s->cacheValid=1;
}
static DWORD rowForPid(const PROCDATA *rows, DWORD count, NP_U32 pid)
{
    DWORD i;
    for (i=0;i<count;++i) if ((rows[i].pid&0xffff)==pid) break;
    return i;
}
/* 3.1/3.5 records carry no handle count, so count class 16 entries (16 bytes).
 * Only the low word of the first field is the PID on every version. */
static LONG backendReadHandles(PROCDATA *rows, DWORD count)
{
    DWORD length=0, handles, matched, i, j;
    NP_U32 pid, last;
    LONG status;
    status=variableQuery(16,&length);
    if (status<0) return status;
    if (length<4) return (LONG)0xc000000dL;
    handles=npWord(queryBuffer,0);
    if (handles>(length-4)/16) return (LONG)0xc000000dL;
    last=0x10000; j=count; matched=0;
    for (i=0;i<handles;++i) {
        pid=npWord(queryBuffer,4+i*16)&0xffff;
        if (pid!=last) { j=rowForPid(rows,count,pid); last=pid; }
        if (j<count) { ++rows[j].handles; ++matched; }
    }
    return handles && !matched ? (LONG)0xc000000dL : 0;
}
