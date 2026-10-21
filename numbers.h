#ifndef NTPERF_NUMBERS_H
#define NTPERF_NUMBERS_H
/* Width follows the selected target. NT 3.x stays C89/32-bit; Win64 keeps
 * full SIZE_T memory values, including values above 4 TiB in KiB units. */
#if defined(_WIN64) || defined(NTPERF_TEST64)
#ifdef __GNUC__
__extension__ typedef unsigned long long NP_SIZE;
__extension__ typedef long long NP_SSIZE;
#else
typedef unsigned __int64 NP_SIZE;
typedef __int64 NP_SSIZE;
#endif
#else
typedef unsigned int NP_SIZE;
typedef int NP_SSIZE;
#endif
#ifndef NTPERF_BACKEND
/* Avoid wsprintf's architecture-dependent 64-bit format support and any CRT. */
static void formatSize(char *out, NP_SIZE value)
{
    char reverse[24];
    int n=0;
    do { reverse[n++]=(char)('0'+value%10); value/=10; } while (value);
    while (n) *out++=reverse[--n];
    *out=0;
}
#endif
#endif
