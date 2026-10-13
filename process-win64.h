/* Modern x64 SYSTEM_PROCESS_INFORMATION. Byte readers avoid packing and
 * unaligned loads. All pointers are validated before conversion to offsets.
 * Layout: Microsoft winternl SYSTEM_PROCESS_INFORMATION, with time fields
 * identified by phnt ntexapi.h. Full fixed header is 0x100 bytes. */
static NP_U32 npWord(const unsigned char *p, NP_U32 at)
{
    return (NP_U32)p[at] | ((NP_U32)p[at+1]<<8) |
           ((NP_U32)p[at+2]<<16) | ((NP_U32)p[at+3]<<24);
}
static NP_SIZE npWide(const unsigned char *p, NP_U32 at)
{
    return (NP_SIZE)npWord(p,at) | ((NP_SIZE)npWord(p,at+4)<<32);
}
static TIME64 npTime(const unsigned char *p, NP_U32 at)
{
    TIME64 t;
    t.low=npWord(p,at); t.high=npWord(p,at+4);
    return t;
}
static int decodeProcess(const unsigned char *buffer, NP_U32 length,
                         NP_SIZE base, NP_U32 pos, PROCDATA *p, NP_U32 *next)
{
    const unsigned char *r;
    NP_U32 bytes, maximum;
    NP_SIZE address, offset, pid;
    if (pos>length || length-pos<0x100) return 0;
    r=buffer+pos;
    *next=npWord(r,0);
    if (*next && (*next<0x100 || (*next&7) || *next>length-pos)) return 0;
    bytes=(NP_U32)r[0x38] | ((NP_U32)r[0x39]<<8);
    maximum=(NP_U32)r[0x3a] | ((NP_U32)r[0x3b]<<8);
    address=npWide(r,0x40);
    p->nameOffset=0; p->nameBytes=bytes;
    if (bytes) {
        if ((bytes&1) || (address&1) || bytes>maximum || address<base) return 0;
        offset=address-base;
        if (offset>length || bytes>length-offset) return 0;
        p->nameOffset=(NP_U32)offset;
    }
    pid=npWide(r,0x50);
    if (pid>(NP_SIZE)0xffffffffUL) return 0; /* OpenProcess takes a DWORD PID. */
    p->threads=npWord(r,4);
    p->created=npTime(r,0x20);
    p->user=npTime(r,0x28);
    p->kernel=npTime(r,0x30);
    p->pid=(NP_U32)pid;
    p->workingKB=npWide(r,0x90)/1024;
    p->virtualKB=npWide(r,0x78)/1024;
    return 1;
}
