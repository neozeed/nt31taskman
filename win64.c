#ifndef _WIN64
#error win64.c requires an x64 target
#endif
#define NTPERF_BACKEND
#define PSAPI_VERSION 1
#include "backend.h"
#include <psapi.h>
#include "process-win64.h"
typedef LONG (WINAPI *VERSION_QUERY)(OSVERSIONINFOW *);
typedef WORD (WINAPI *GROUP_COUNT)(void);
static int multipleGroups;
static int backendCpuSupported(void) { return !multipleGroups; }
/* The x64 record already carries HandleCount. */
static LONG backendReadHandles(PROCDATA *rows, DWORD count) { (void)rows; (void)count; return 0; }
#include "native-query.inc"
int backendInit(DWORD *version)
{
    union { FARPROC generic; VERSION_QUERY version; GROUP_COUNT groups; } fn;
    OSVERSIONINFOW info;
    *version=GetVersion();
    if (!initNative()) return 0;
    /* GetVersion can report 6.2 on newer Windows without a manifest. */
    fn.generic=GetProcAddress(GetModuleHandleA("ntdll.dll"),"RtlGetVersion");
    if (fn.generic) {
        clearBytes(&info,sizeof(info)); info.dwOSVersionInfoSize=sizeof(info);
        if (fn.version(&info)>=0)
            *version=(info.dwMajorVersion&255)|((info.dwMinorVersion&255)<<8)|
                     ((info.dwBuildNumber&65535)<<16);
    }
    fn.generic=GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetActiveProcessorGroupCount");
    if (fn.generic) multipleGroups=fn.groups()>1;
    return 1;
}
const char *backendName(void)
{
    return multipleGroups ? "Win64 (multiple CPU groups: CPU metrics unavailable)" : "Win64";
}
void backendReadMemory(SAMPLE *s, const SYSTEM_INFO *info)
{
    PERFORMANCE_INFORMATION p;
    NP_SIZE pageKB;
    (void)info;
    clearBytes(&p,sizeof(p)); p.cb=sizeof(p);
    s->memoryValid=GetPerformanceInfo(&p,sizeof(p))!=0;
    s->memoryError=s->memoryValid ? 0 : GetLastError();
    s->perfStatus=s->memoryValid ? 0 : (LONG)0xc0000001L;
    s->cacheValid=s->memoryValid;
    if (!s->memoryValid) return;
    pageKB=(NP_SIZE)p.PageSize/1024;
    s->totalKB=(NP_SIZE)p.PhysicalTotal*pageKB;
    s->availableKB=(NP_SIZE)p.PhysicalAvailable*pageKB;
    s->cacheKB=(NP_SIZE)p.SystemCache*pageKB;
    s->commitKB=(NP_SIZE)p.CommitTotal*pageKB;
    s->limitKB=(NP_SIZE)p.CommitLimit*pageKB;
    s->peakKB=(NP_SIZE)p.CommitPeak*pageKB;
    s->pagedKB=(NP_SIZE)p.KernelPaged*pageKB;
    s->nonpagedKB=(NP_SIZE)p.KernelNonpaged*pageKB;
}
