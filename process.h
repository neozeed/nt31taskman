/* Normalized process data shared by both backends and the UI. */
#ifndef NTPERF_PROCESS_H
#define NTPERF_PROCESS_H
#include "counters.h"
#include "numbers.h"
typedef struct {
    NP_U32 pid, threads, nameOffset, nameBytes, faults, priority, handles;
    NP_SIZE workingKB, virtualKB, peakKB, commitKB, pagedKB, nonpagedKB;
    char name[260];
    TIME64 created, user, kernel;
} PROCDATA;
#ifndef NTPERF_BACKEND
static int sameProcess(const PROCDATA *a, const PROCDATA *b)
{
    return a->pid==b->pid && a->created.low==b->created.low &&
           a->created.high==b->created.high;
}
static double processTime(const PROCDATA *p)
{
    return ((double)p->user.high+(double)p->kernel.high)*4294967296.0 +
           (double)p->user.low+(double)p->kernel.low;
}
#endif
#endif
