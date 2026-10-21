/* Released NT 3.1/3.5 x86 layout, isolated from the UI. */
static NP_U32 npWord(const unsigned char *p, NP_U32 at)
{
    return (NP_U32)p[at] | ((NP_U32)p[at+1]<<8) |
           ((NP_U32)p[at+2]<<16) | ((NP_U32)p[at+3]<<24);
}
static TIME64 npTime(const unsigned char *p, NP_U32 at)
{
    TIME64 t;
    t.low=npWord(p,at); t.high=npWord(p,at+4);
    return t;
}
static int decodeProcess(const unsigned char *buffer, NP_U32 length,
                         NP_U32 base, NP_U32 pos, PROCDATA *p, NP_U32 *next)
{
    const unsigned char *r;
    NP_U32 bytes, maximum, address;
    if (pos > length || length-pos < 0x88) return 0;
    r=buffer+pos;
    *next=npWord(r,0);
    if (*next && (*next<0x88 || (*next&3) || *next>length-pos)) return 0;
    bytes=(NP_U32)r[0x38] | ((NP_U32)r[0x39]<<8);
    maximum=(NP_U32)r[0x3a] | ((NP_U32)r[0x3b]<<8);
    address=npWord(r,0x3c);
    p->nameOffset=0; p->nameBytes=bytes;
    if (bytes) {
        if ((bytes&1) || (address&1) || bytes>maximum || address<base) return 0;
        p->nameOffset=address-base;
        if (p->nameOffset>length || bytes>length-p->nameOffset) return 0;
    }
    p->threads=npWord(r,4);
    p->created=npTime(r,0x20);
    p->user=npTime(r,0x28);
    p->kernel=npTime(r,0x30);
    p->pid=npWord(r,0x44);
    p->priority=npWord(r,0x40);
    p->faults=npWord(r,0x60);
    p->handles=0;
    p->workingKB=npWord(r,0x68)/1024;
    p->virtualKB=npWord(r,0x5c)/1024;
    p->peakKB=npWord(r,0x64)/1024;
    p->pagedKB=npWord(r,0x70)/1024;
    p->nonpagedKB=npWord(r,0x78)/1024;
    p->commitKB=npWord(r,0x84)/1024;
    return 1;
}
