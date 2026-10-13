#ifdef _WIN64
#error nt31.c requires the x86 NT 3.1 target
#endif
#define NTPERF_BACKEND
#include "backend.h"
#include "process-nt31.h"
static int backendCpuSupported(void) { return 1; }
#include "native-query.inc"
int backendInit(DWORD *version)
{
    *version=GetVersion();
    return initNative();
}
const char *backendName(void) { return "NT 3.1 x86"; }
void backendReadMemory(SAMPLE *s, const SYSTEM_INFO *info)
{
    DWORD perf[78], length;
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

    s->handlesValid=0;
    s->handleStatus=variableQuery(16,&length);
    if (s->handleStatus>=0 && length>=4) {
        s->handles=npWord(queryBuffer,0); s->handlesValid=1;
    }
}
