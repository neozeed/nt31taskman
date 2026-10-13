/* C89 counter arithmetic shared by the monitor and native regression test. */
#ifndef NTPERF_COUNTERS_H
#define NTPERF_COUNTERS_H
/* Windows ILP32 and our test host both provide a 32-bit unsigned int. */
typedef unsigned int NP_U32;
typedef char CHECK_NP_U32[(sizeof(NP_U32) == 4) ? 1 : -1];
typedef struct { NP_U32 low, high; } TIME64;
typedef struct {
    TIME64 idle, kernel, user, dpc, interrupt;
    NP_U32 interrupts, padding;
} CPUINFO;
typedef char CHECK_CPUINFO[(sizeof(CPUINFO) == 48) ? 1 : -1];

#ifndef NTPERF_BACKEND
static double timeDelta(TIME64 now, TIME64 old)
{
    NP_U32 low, high;
    if (now.high < old.high ||
        (now.high == old.high && now.low < old.low)) return -1.0;
    low = now.low - old.low;
    high = now.high - old.high - (now.low < old.low ? 1 : 0);
    return (double)high * 4294967296.0 + (double)low;
}

static int percentage(double value, double total)
{
    double result;
    if (total <= 0.0 || value < 0.0) return -1;
    result = 100.0 * value / total;
    if (result > 100.0) result = 100.0;
    return (int)(result + 0.5);
}

static void cpuPercentages(const CPUINFO *now, const CPUINFO *old,
                           unsigned int count, int *busy, int *busyKernel)
{
    unsigned int i;
    double idle, kernel, user, di, dk, du, total;
    *busy = *busyKernel = -1;
    idle = kernel = user = 0.0;
    for (i = 0; i < count; ++i) {
        di = timeDelta(now[i].idle, old[i].idle);
        dk = timeDelta(now[i].kernel, old[i].kernel);
        du = timeDelta(now[i].user, old[i].user);
        if (di < 0.0 || dk < 0.0 || du < 0.0) return;
        idle += di; kernel += dk; user += du;
    }
    total = kernel + user;
    /* Counters are sampled at slightly different instants; clamp rounding. */
    if (kernel < idle) kernel = idle;
    *busy = percentage(kernel + user - idle, total);
    *busyKernel = percentage(kernel - idle, total);
}
#endif /* arithmetic used by UI/tests */
#endif
