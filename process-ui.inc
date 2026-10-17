/* Included by ntperf31.c: classic owner-drawn listbox, no COMCTL32. */
#define ID_PROCESSES 200
#define ID_PERFORMANCE 201
#define ID_ENDPROCESS 202
#define ID_LIST 203
#define ID_COLUMN 210
#define COLUMNS 7
static HWND processList, endButton, tabs[2], headers[COLUMNS];
static const char *columnNames[COLUMNS] = {
    "Image Name", "PID", "CPU", "CPU Time", "Mem Usage", "VM Size", "Threads"
};
static int columnWidths[COLUMNS]={170,48,42,78,78,78,55};
static int activeTab=0, sortColumn=0, sortDescending=0, endingProcess=0;
static int rowHeight=18;
typedef struct { PROCDATA data; char name[260]; int cpu; } PROCESSROW;
static PROCESSROW *processRows;
static DWORD processCount, processTick;
static int processBaseline;
static void updateProcessControls(void);
static void layoutProcessControls(void);
static void takeSample(void);

static int protectedProcess(const PROCESSROW *p)
{
    static const char *names[]={"System","System Process","System Idle Process",
        "Idle","smss.exe","csrss.exe","winlogon.exe","lsass.exe","services.exe"};
    int i;
    if (!p->data.pid || ((version&65535)==0x0a03 && p->data.pid==2) ||
        p->data.pid==GetCurrentProcessId() ||
        (!p->data.created.low && !p->data.created.high)) return 1;
    for (i=0; i<(int)(sizeof(names)/sizeof(names[0])); ++i)
        if (!lstrcmpiA(p->name,names[i])) return 1;
    return 0;
}
static int rowCompare(const PROCESSROW *a, const PROCESSROW *b)
{
    int result;
    double x,y;
    x=y=0.0; result=0;
    if (sortColumn==0) result=lstrcmpiA(a->name,b->name);
    else {
        if (sortColumn==1) { x=a->data.pid; y=b->data.pid; }
        if (sortColumn==2) { x=a->cpu; y=b->cpu; }
        if (sortColumn==3) { x=processTime(&a->data); y=processTime(&b->data); }
        if (sortColumn==4) { x=a->data.workingKB; y=b->data.workingKB; }
        if (sortColumn==5) { x=a->data.virtualKB; y=b->data.virtualKB; }
        if (sortColumn==6) { x=a->data.threads; y=b->data.threads; }
        result=x<y ? -1 : (x>y ? 1 : 0);
    }
    if (sortDescending) result=-result;
    if (!result) result=a->data.pid<b->data.pid ? -1 : (a->data.pid>b->data.pid ? 1 : 0);
    return result;
}
static void sortRows(void)
{
    DWORD i,j;
    PROCESSROW value;
    for (i=1;i<processCount;++i) {
        value=processRows[i]; j=i;
        while (j && rowCompare(&value,&processRows[j-1])<0) {
            processRows[j]=processRows[j-1]; --j;
        }
        processRows[j]=value;
    }
}
static DWORD cpuSeconds(const PROCDATA *p)
{
    return (DWORD)(processTime(p)/10000000.0);
}
static int rowChanged(const PROCESSROW *a, const PROCESSROW *b)
{
    return !sameProcess(&a->data,&b->data) || a->cpu!=b->cpu ||
        a->data.threads!=b->data.threads || a->data.workingKB!=b->data.workingKB ||
        a->data.virtualKB!=b->data.virtualKB || cpuSeconds(&a->data)!=cpuSeconds(&b->data) ||
        lstrcmpA(a->name,b->name);
}
/* No reset: it erased the whole list every tick. Add/delete erase only their rows;
 * UpdateWindow flushes that so changed rows repaint without erase. */
static void populateList(const PROCDATA *selected, const PROCESSROW *old, DWORD oldCount)
{
    DWORD i;
    int items, selection, top;
    RECT r;
    if (!processList) return;
    items=(int)SendMessageA(processList,LB_GETCOUNT,0,0);
    for (;items<(int)processCount;++items)
        if (SendMessageA(processList,LB_ADDSTRING,0,0)<0) break;
    while (items>(int)processCount) SendMessageA(processList,LB_DELETESTRING,(WPARAM)--items,0);
    selection=-1;
    for (i=0;selected && i<processCount;++i)
        if (sameProcess(selected,&processRows[i].data)) { selection=(int)i; break; }
    if ((int)SendMessageA(processList,LB_GETCURSEL,0,0)!=selection) {
        top=(int)SendMessageA(processList,LB_GETTOPINDEX,0,0);
        SendMessageA(processList,LB_SETCURSEL,(WPARAM)selection,0);
        SendMessageA(processList,LB_SETTOPINDEX,(WPARAM)top,0);
    }
    UpdateWindow(processList);
    for (i=0;i<processCount;++i) {
        if (i<oldCount && !rowChanged(&processRows[i],&old[i])) continue;
        if (SendMessageA(processList,LB_GETITEMRECT,(WPARAM)i,(LPARAM)&r)!=LB_ERR)
            InvalidateRect(processList,&r,FALSE);
    }
    updateProcessControls();
}
static void collectProcesses(const PROCDATA *data, DWORD count)
{
    DWORD i, j, now, elapsed, oldCount;
    PROCESSROW *rows, *oldRows;
    PROCDATA selected;
    int selection, haveSelection;
    double userDelta,kernelDelta;
    rows=(PROCESSROW *)GlobalAlloc(GMEM_FIXED,count*sizeof(PROCESSROW));
    if (!rows) goto malformed;
    now=GetTickCount(); elapsed=now-processTick;
    selection=processList ? (int)SendMessageA(processList,LB_GETCURSEL,0,0) : -1;
    haveSelection=selection>=0 && (DWORD)selection<processCount;
    if (haveSelection) selected=processRows[selection].data;
    sample.threads=0;
    for (i=0;i<count;++i) {
        rows[i].data=data[i];
        rows[i].cpu=BAD;
        lstrcpyA(rows[i].name,data[i].name);
        if (!rows[i].name[0]) {
            if (!rows[i].data.pid) lstrcpyA(rows[i].name,"System Idle Process");
            else wsprintfA(rows[i].name,"Process %lu",(DWORD)rows[i].data.pid);
        }
        if (processBaseline && elapsed && sample.cpuStatus>=0 && sysInfo.dwNumberOfProcessors) {
            for (j=0;j<processCount;++j) {
                if (!sameProcess(&rows[i].data,&processRows[j].data)) continue;
                userDelta=timeDelta(rows[i].data.user,processRows[j].data.user);
                kernelDelta=timeDelta(rows[i].data.kernel,processRows[j].data.kernel);
                if (userDelta>=0.0 && kernelDelta>=0.0)
                    rows[i].cpu=percentage(userDelta+kernelDelta,
                        (double)elapsed*10000.0*(double)sysInfo.dwNumberOfProcessors);
                break;
            }
        }
        sample.threads+=rows[i].data.threads;
    }
    oldRows=processRows; oldCount=processCount;
    processRows=rows; processCount=count;
    processTick=now; processBaseline=1;
    sample.processes=count; sample.totalsValid=1;
    sortRows();
    populateList(haveSelection?&selected:NULL,oldRows,oldCount);
    if (oldRows) GlobalFree((HGLOBAL)oldRows);
    return;
malformed:
    sample.processStatus=(LONG)0xc000000dL;
    sample.totalsValid=0; processBaseline=0;
    updateProcessControls();
}
static void updateProcessControls(void)
{
    int selection;
    if (!endButton) return;
    selection=(int)SendMessageA(processList,LB_GETCURSEL,0,0);
    EnableWindow(endButton,sample.totalsValid && selection>=0 &&
        (DWORD)selection<processCount && !protectedProcess(&processRows[selection]));
}
static void setProcessTab(int tab)
{
    int i;
    activeTab=tab;
    ShowWindow(processList,tab==0?SW_SHOW:SW_HIDE);
    ShowWindow(endButton,tab==0?SW_SHOW:SW_HIDE);
    for(i=0;i<COLUMNS;++i) ShowWindow(headers[i],tab==0?SW_SHOW:SW_HIDE);
    InvalidateRect(mainWindow,NULL,FALSE);
    InvalidateRect(tabs[0],NULL,TRUE); InvalidateRect(tabs[1],NULL,TRUE);
    SetFocus(tab==0?processList:tabs[1]);
}
static void layoutProcessControls(void)
{
    RECT r;
    int i,x,width;
    if (!processList) return;
    GetClientRect(mainWindow,&r);
    width=r.right-26-GetSystemMetrics(SM_CXVSCROLL);
    /* Numeric columns total 379 px; name column receives the remainder. */
    columnWidths[0]=width-379;
    if (columnWidths[0] < 55) columnWidths[0]=55;
    x=12;
    for (i=0;i<COLUMNS;++i) {
        MoveWindow(headers[i],x,42,columnWidths[i],22,TRUE);
        x+=columnWidths[i];
    }
    MoveWindow(processList,12,64,r.right-24,r.bottom-132,TRUE);
    MoveWindow(endButton,r.right-125,r.bottom-58,113,25,TRUE);
}
static void drawProcessItem(const DRAWITEMSTRUCT *d)
{
    PROCESSROW *p;
    RECT cell, focus;
    HFONT old;
    COLORREF bg,fg;
    char buffer[300];
    DWORD seconds;
    int i,x;
    if (d->itemID==(UINT)-1 || d->itemID>=processCount) return;
    p=&processRows[d->itemID];
    bg=GetSysColor((d->itemState&ODS_SELECTED)?COLOR_HIGHLIGHT:COLOR_WINDOW);
    fg=GetSysColor((d->itemState&ODS_SELECTED)?COLOR_HIGHLIGHTTEXT:COLOR_WINDOWTEXT);
    SetBkColor(d->hDC,bg); SetTextColor(d->hDC,fg);
    ExtTextOutA(d->hDC,0,0,ETO_OPAQUE,&d->rcItem,"",0,NULL);
    old=(HFONT)SelectObject(d->hDC,uiFont);
    SetBkMode(d->hDC,TRANSPARENT);
    x=d->rcItem.left;
    seconds=cpuSeconds(&p->data);
    for(i=0;i<COLUMNS;++i) {
        if(i==0) lstrcpyA(buffer,p->name);
        if(i==1) wsprintfA(buffer,"%lu",(DWORD)p->data.pid);
        if(i==2) { if(p->cpu<0) lstrcpyA(buffer,"--"); else wsprintfA(buffer,"%d",p->cpu); }
        if(i==3) wsprintfA(buffer,"%lu:%02lu:%02lu",seconds/3600,(seconds/60)%60,seconds%60);
        if(i==4) { formatSize(buffer,p->data.workingKB); lstrcatA(buffer," K"); }
        if(i==5) { formatSize(buffer,p->data.virtualKB); lstrcatA(buffer," K"); }
        if(i==6) wsprintfA(buffer,"%lu",(DWORD)p->data.threads);
        SetRect(&cell,x+4,d->rcItem.top,x+columnWidths[i]-4,d->rcItem.bottom);
        DrawTextA(d->hDC,buffer,-1,&cell,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|(i?DT_RIGHT:DT_LEFT));
        x+=columnWidths[i];
    }
    if(d->itemState&ODS_FOCUS) { focus=d->rcItem; DrawFocusRect(d->hDC,&focus); }
    SelectObject(d->hDC,old);
}
static void drawTab(const DRAWITEMSTRUCT *d)
{
    RECT r;
    HBRUSH brush;
    HFONT old;
    int selected;
    selected=(d->CtlID==ID_PROCESSES && activeTab==0)||
             (d->CtlID==ID_PERFORMANCE && activeTab==1);
    r=d->rcItem;
    brush=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    FillRect(d->hDC,&r,brush); DeleteObject(brush);
    FrameRect(d->hDC,&r,(HBRUSH)GetStockObject(selected?WHITE_BRUSH:GRAY_BRUSH));
    old=(HFONT)SelectObject(d->hDC,uiFont);
    SetBkMode(d->hDC,TRANSPARENT); SetTextColor(d->hDC,GetSysColor(COLOR_BTNTEXT));
    DrawTextA(d->hDC,d->CtlID==ID_PROCESSES?"Processes":"Performance",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if(d->itemState&ODS_FOCUS) { InflateRect(&r,-3,-3); DrawFocusRect(d->hDC,&r); }
    SelectObject(d->hDC,old);
}
static void sortByColumn(int column)
{
    int selection,valid;
    PROCDATA selected;
    selection=(int)SendMessageA(processList,LB_GETCURSEL,0,0);
    valid=selection>=0 && (DWORD)selection<processCount;
    if(valid) selected=processRows[selection].data;
    if(sortColumn==column) sortDescending=!sortDescending;
    else { sortColumn=column; sortDescending=column>=2; }
    sortRows(); populateList(valid?&selected:NULL,NULL,0);
}
static void endSelectedProcess(void)
{
    PROCESSROW target;
    int selected,answer;
    HANDLE handle;
    FILETIME created,exited,kernel,user;
    char prompt[600];
    DWORD error;
    if (endingProcess || !sample.totalsValid) return;
    selected=(int)SendMessageA(processList,LB_GETCURSEL,0,0);
    if(selected<0 || (DWORD)selected>=processCount) return;
    target=processRows[selected];
    if(protectedProcess(&target)) return;
    /* Snapshot selection and hold an object handle across the modal dialog. */
    endingProcess=1;
    handle=OpenProcess(PROCESS_TERMINATE|PROCESS_QUERY_INFORMATION,FALSE,target.data.pid);
    if (!handle) {
        error=GetLastError();
        wsprintfA(prompt,"Cannot open %s (PID %lu).\nWindows error %lu.",target.name,(DWORD)target.data.pid,error);
        MessageBoxA(mainWindow,prompt,"End Process",MB_OK|MB_ICONEXCLAMATION);
    } else {
        if(!GetProcessTimes(handle,&created,&exited,&kernel,&user)) {
            error=GetLastError();
            wsprintfA(prompt,"Cannot verify the selected process.\nWindows error %lu. Nothing was terminated.",error);
            MessageBoxA(mainWindow,prompt,"End Process",MB_OK|MB_ICONEXCLAMATION);
        } else if(created.dwLowDateTime!=target.data.created.low ||
                  created.dwHighDateTime!=target.data.created.high) {
            MessageBoxA(mainWindow,"The selected process has exited and its PID was reused.\nNothing was terminated.","End Process",MB_OK|MB_ICONEXCLAMATION);
        } else {
            wsprintfA(prompt,"End %s (PID %lu)?\n\nThe process will be terminated immediately.\nUnsaved work in that process will be lost.",target.name,(DWORD)target.data.pid);
            answer=MessageBoxA(mainWindow,prompt,"End Process",MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2);
            if(answer==IDYES && !TerminateProcess(handle,1)) {
                error=GetLastError();
                wsprintfA(prompt,"The process could not be terminated.\nWindows error %lu.",error);
                MessageBoxA(mainWindow,prompt,"End Process",MB_OK|MB_ICONEXCLAMATION);
            }
        }
        CloseHandle(handle);
    }
    endingProcess=0;
    takeSample();
}
static void createProcessControls(void)
{
    int i;
    HDC dc;
    TEXTMETRICA tm;
    HFONT old;
    tabs[0]=CreateWindowA("BUTTON","Processes",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
        10,8,90,26,mainWindow,(HMENU)ID_PROCESSES,instance,NULL);
    tabs[1]=CreateWindowA("BUTTON","Performance",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
        100,8,100,26,mainWindow,(HMENU)ID_PERFORMANCE,instance,NULL);
    dc=GetDC(mainWindow); old=(HFONT)SelectObject(dc,uiFont);
    GetTextMetricsA(dc,&tm); rowHeight=tm.tmHeight+4;
    SelectObject(dc,old); ReleaseDC(mainWindow,dc);
    for(i=0;i<COLUMNS;++i) headers[i]=CreateWindowA("BUTTON",columnNames[i],
        WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,0,1,1,mainWindow,(HMENU)(WPARAM)(ID_COLUMN+i),instance,NULL);
    processList=CreateWindowA("LISTBOX","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_BORDER|WS_VSCROLL|
        LBS_OWNERDRAWFIXED|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,0,0,1,1,mainWindow,(HMENU)ID_LIST,instance,NULL);
    endButton=CreateWindowA("BUTTON","&End Process",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
        0,0,1,1,mainWindow,(HMENU)ID_ENDPROCESS,instance,NULL);
    SendMessageA(processList,WM_SETFONT,(WPARAM)uiFont,FALSE);
    SendMessageA(endButton,WM_SETFONT,(WPARAM)uiFont,FALSE);
    for(i=0;i<COLUMNS;++i) SendMessageA(headers[i],WM_SETFONT,(WPARAM)uiFont,FALSE);
    layoutProcessControls(); updateProcessControls();
}
