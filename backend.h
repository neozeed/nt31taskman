#ifndef NTPERF_BACKEND_H
#define NTPERF_BACKEND_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "process.h"
#ifdef _WIN64
#define CPU_MAX 64
#define APP_TITLE "NT Performance - Win64"
typedef INT_PTR NP_DLGRESULT;
#else
#define CPU_MAX 32
#define APP_TITLE "NT Performance - Windows NT 3.1"
typedef BOOL NP_DLGRESULT;
#endif
typedef struct {
    int cpu, kernel, memoryValid, totalsValid, handlesValid;
    NP_SIZE totalKB, availableKB, commitKB, limitKB, peakKB;
    NP_SIZE pagedKB, nonpagedKB;
    DWORD processes, threads, handles;
    LONG cpuStatus, perfStatus, processStatus, handleStatus, processQueryStatus;
    DWORD memoryError;
} SAMPLE;
int backendInit(DWORD *version);
const char *backendName(void);
void backendShutdown(void);
LONG backendReadCpu(CPUINFO *current, DWORD count);
void backendReadMemory(SAMPLE *s, const SYSTEM_INFO *info);
/* Caller owns the normalized allocation (GlobalFree); no native pointers
 * escape the backend. queryStatus distinguishes OS failure from bad data. */
LONG backendReadProcesses(PROCDATA **out, DWORD *count, LONG *queryStatus);
#endif
