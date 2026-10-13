/* Private helpers shared by the two backend translation units. */
typedef LONG (WINAPI *QUERY)(ULONG, PVOID, ULONG, PULONG);
typedef union { FARPROC generic; QUERY query; } QUERY_CAST;
static QUERY query;
static BYTE *queryBuffer;
static DWORD queryCapacity;
#define MISMATCH ((LONG)0xc0000004L)
static void clearBytes(void *p, DWORD n)
{
    BYTE *b=(BYTE *)p;
    while (n--) *b++=0;
}
static int initNative(void)
{
    QUERY_CAST convert;
    HMODULE module=GetModuleHandleA("ntdll.dll");
    convert.generic=module ? GetProcAddress(module,"NtQuerySystemInformation") : NULL;
    query=convert.query;
    return query != NULL;
}
void backendShutdown(void)
{
    if (queryBuffer) GlobalFree((HGLOBAL)queryBuffer);
    queryBuffer=NULL; queryCapacity=0;
}
/* Class 5/16 requests can race process creation. Grow and retry, bounded. */
static LONG variableQuery(ULONG kind, DWORD *length)
{
    LONG status;
    ULONG needed;
    DWORD capacity;
    BYTE *replacement;
    for (;;) {
        if (!queryCapacity) {
            queryCapacity = 65536;
            queryBuffer = (BYTE *)GlobalAlloc(GMEM_FIXED, queryCapacity);
            if (!queryBuffer) { queryCapacity = 0; return (LONG)0xc0000017L; }
        }
        needed = 0;
        status = query(kind, queryBuffer, queryCapacity, &needed);
        if (status >= 0) {
            *length = needed ? needed : queryCapacity;
            if (*length > queryCapacity) return (LONG)0xc000000dL;
            return status;
        }
        if (status != MISMATCH && status != (LONG)0xc0000023L) return status;
        capacity = queryCapacity * 2;
        if (needed > capacity) capacity = needed;
        if (capacity > 16UL * 1024UL * 1024UL) return status;
        replacement = (BYTE *)GlobalAlloc(GMEM_FIXED, capacity);
        if (!replacement) return (LONG)0xc0000017L;
        GlobalFree((HGLOBAL)queryBuffer);
        queryBuffer = replacement;
        queryCapacity = capacity;
    }
}


LONG backendReadCpu(CPUINFO *current, DWORD count)
{
    ULONG returned=0;
    LONG status;
    if (!count || count>CPU_MAX || !backendCpuSupported()) return (LONG)0xc00000bbL;
    clearBytes(current,count*sizeof(CPUINFO));
    status=query(8,current,count*sizeof(CPUINFO),&returned);
    if (status>=0 && returned && returned!=count*sizeof(CPUINFO)) return (LONG)0xc000000dL;
    return status;
}
LONG backendReadProcesses(PROCDATA **out, DWORD *outCount, LONG *queryStatus)
{
    DWORD length=0, pos, count, i;
    NP_U32 next;
    PROCDATA decoded, *rows;
    int chars;
    *out=NULL; *outCount=0;
    *queryStatus=variableQuery(5,&length);
    if (*queryStatus<0) return *queryStatus;
    pos=count=0;
    do {
        if (!decodeProcess(queryBuffer,length,(NP_SIZE)queryBuffer,pos,&decoded,&next))
            return (LONG)0xc000000dL;
        ++count; pos+=next;
    } while (next);
    rows=(PROCDATA *)GlobalAlloc(GMEM_FIXED,count*sizeof(PROCDATA));
    if (!rows) return (LONG)0xc0000017L;
    pos=0;
    for (i=0;i<count;++i) {
        if (!decodeProcess(queryBuffer,length,(NP_SIZE)queryBuffer,pos,&rows[i],&next)) {
            GlobalFree((HGLOBAL)rows); return (LONG)0xc000000dL;
        }
        chars=(int)(rows[i].nameBytes/2);
        if (chars>250) chars=250;
        if (chars) chars=WideCharToMultiByte(CP_ACP,0,
            (LPCWSTR)(queryBuffer+rows[i].nameOffset),chars,rows[i].name,259,NULL,NULL);
        rows[i].name[chars>0 ? chars : 0]=0;
        pos+=next;
    }
    *out=rows; *outCount=count;
    return 0;
}
