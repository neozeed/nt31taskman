/* Included by ntperf3x.c: classic owner-drawn listbox, no COMCTL32. */
#define ID_TAB 200
#define ID_ENDPROCESS 203
#define ID_LIST 204
#define ID_PRIORITY 205
#define ID_COLUMN 210
#define ID_SHOWCOLUMN 230
#define TABS 3
enum { TAB_APPS, TAB_PROCESSES, TAB_PERFORMANCE };
enum { COL_NAME, COL_PID, COL_CPU, COL_TIME, COL_MEM, COL_MEMDELTA, COL_PEAK, COL_FAULTS,
       COL_FAULTDELTA, COL_VM, COL_COMMIT, COL_PAGED, COL_NONPAGED, COL_PRIORITY,
       COL_HANDLES, COL_THREADS, COLUMNS };
static HWND processList, endButton, tabs[TABS], headers[COLUMNS];
static HMENU processMenu;
static const char *tabNames[TABS]={"Applications","Processes","Performance"};
static const char *columnNames[COLUMNS]={
    "Image Name", "PID", "CPU", "CPU Time", "Mem Usage", "Mem Delta", "Peak Mem", "Page Faults",
    "PF Delta", "VM Size", "Commit", "Paged Pool", "NP Pool", "Base Pri", "Handles", "Threads"
};
static int columnWidths[COLUMNS]={170,48,42,78,78,70,78,74,60,78,78,70,62,78,58,55};
static int columnShown[COLUMNS]={1,1,1,1,1,0,0,0,0,1,0,0,0,0,0,1};
static const char *priorityNames[6]={"Idle","Normal","High","Realtime","Below Normal","Above Normal"};
static int activeTab=TAB_PROCESSES, sortColumn=COL_NAME, sortDescending=0, processBusy=0;
static int rowHeight=18;
typedef struct {
    PROCDATA data; char name[260];
    int cpu, hasDelta, hasHandles;
    NP_SSIZE memDelta;
    LONG faultDelta;
} PROCESSROW;
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
/* Base priority to a priorityNames index, as NT 4 Task Manager does: idle 4,
 * normal 7-9, high 13, realtime 24. Windows 2000 adds below normal 6 and
 * above normal 10, which the Set Priority menu does not offer. */
static int priorityLevel(NP_U32 priority)
{
    int classes2000=LOBYTE(LOWORD(version))>=5;
    if (priority<=4) return 0;
    if (classes2000 && priority<=6) return 4;
    if (priority<=9) return 1;
    if (classes2000 && priority<=11) return 5;
    return priority<16 ? 2 : 3;
}
static DWORD cpuSeconds(const PROCDATA *p)
{
    return (DWORD)(processTime(p)/10000000.0);
}
static void kilobytes(char *buffer, NP_SIZE value)
{
    formatSize(buffer,value); lstrcatA(buffer," K");
}
static void signedKilobytes(char *buffer, NP_SSIZE value)
{
    if (value<0) *buffer++='-';
    kilobytes(buffer,value<0 ? (NP_SIZE)0-(NP_SIZE)value : (NP_SIZE)value);
}
static void cellText(const PROCESSROW *p, int column, char *buffer)
{
    const PROCDATA *d=&p->data;
    DWORD seconds;
    switch (column) {
    case COL_NAME: lstrcpyA(buffer,p->name); return;
    case COL_PID: wsprintfA(buffer,"%lu",(DWORD)d->pid); return;
    case COL_CPU: if (p->cpu<0) break; wsprintfA(buffer,"%d",p->cpu); return;
    case COL_TIME:
        seconds=cpuSeconds(d);
        wsprintfA(buffer,"%lu:%02lu:%02lu",seconds/3600,(seconds/60)%60,seconds%60);
        return;
    case COL_MEM: kilobytes(buffer,d->workingKB); return;
    case COL_MEMDELTA: if (!p->hasDelta) break; signedKilobytes(buffer,p->memDelta); return;
    case COL_PEAK: kilobytes(buffer,d->peakKB); return;
    case COL_FAULTS: wsprintfA(buffer,"%lu",(DWORD)d->faults); return;
    case COL_FAULTDELTA: if (!p->hasDelta) break; wsprintfA(buffer,"%ld",p->faultDelta); return;
    case COL_VM: kilobytes(buffer,d->virtualKB); return;
    case COL_COMMIT: kilobytes(buffer,d->commitKB); return;
    case COL_PAGED: kilobytes(buffer,d->pagedKB); return;
    case COL_NONPAGED: kilobytes(buffer,d->nonpagedKB); return;
    case COL_PRIORITY: lstrcpyA(buffer,priorityNames[priorityLevel(d->priority)]); return;
    case COL_HANDLES: if (!p->hasHandles) break; wsprintfA(buffer,"%lu",(DWORD)d->handles); return;
    case COL_THREADS: wsprintfA(buffer,"%lu",(DWORD)d->threads); return;
    }
    lstrcpyA(buffer,"--");
}
static double cellValue(const PROCESSROW *p, int column)
{
    const PROCDATA *d=&p->data;
    switch (column) {
    case COL_PID: return d->pid;
    case COL_CPU: return p->cpu;
    case COL_TIME: return processTime(d);
    case COL_MEM: return d->workingKB;
    case COL_MEMDELTA: return (double)p->memDelta;
    case COL_PEAK: return d->peakKB;
    case COL_FAULTS: return d->faults;
    case COL_FAULTDELTA: return p->faultDelta;
    case COL_VM: return d->virtualKB;
    case COL_COMMIT: return d->commitKB;
    case COL_PAGED: return d->pagedKB;
    case COL_NONPAGED: return d->nonpagedKB;
    case COL_PRIORITY: return d->priority;
    case COL_HANDLES: return d->handles;
    case COL_THREADS: return d->threads;
    }
    return 0.0;
}
static int rowCompare(const PROCESSROW *a, const PROCESSROW *b)
{
    int result;
    double x,y;
    if (sortColumn==COL_NAME) result=lstrcmpiA(a->name,b->name);
    else {
        x=cellValue(a,sortColumn); y=cellValue(b,sortColumn);
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
static int rowChanged(const PROCESSROW *a, const PROCESSROW *b)
{
    char x[300], y[300];
    int i;
    if (!sameProcess(&a->data,&b->data)) return 1;
    for (i=0;i<COLUMNS;++i) {
        if (!columnShown[i]) continue;
        cellText(a,i,x); cellText(b,i,y);
        if (lstrcmpA(x,y)) return 1;
    }
    return 0;
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
static const PROCESSROW *previousRow(const PROCDATA *p)
{
    DWORD i;
    for (i=0;i<processCount;++i) if (sameProcess(p,&processRows[i].data)) return &processRows[i];
    return NULL;
}
static void collectProcesses(const PROCDATA *data, DWORD count)
{
    DWORD i, now, elapsed, oldCount;
    PROCESSROW *rows, *oldRows;
    const PROCESSROW *old;
    PROCDATA selected;
    int selection, haveSelection;
    double userDelta,kernelDelta;
    rows=(PROCESSROW *)GlobalAlloc(GMEM_FIXED,count*sizeof(PROCESSROW));
    if (!rows) goto malformed;
    now=GetTickCount(); elapsed=now-processTick;
    selection=processList ? (int)SendMessageA(processList,LB_GETCURSEL,0,0) : -1;
    haveSelection=selection>=0 && (DWORD)selection<processCount;
    if (haveSelection) selected=processRows[selection].data;
    sample.threads=sample.handles=0;
    for (i=0;i<count;++i) {
        rows[i].data=data[i];
        rows[i].cpu=BAD;
        rows[i].hasDelta=0; rows[i].memDelta=rows[i].faultDelta=0;
        rows[i].hasHandles=sample.handleStatus>=0;
        lstrcpyA(rows[i].name,data[i].name);
        if (!rows[i].name[0]) {
            if (!rows[i].data.pid) lstrcpyA(rows[i].name,"System Idle Process");
            else wsprintfA(rows[i].name,"Process %lu",(DWORD)rows[i].data.pid);
        }
        sample.threads+=rows[i].data.threads;
        sample.handles+=rows[i].data.handles;
        old=processBaseline ? previousRow(&rows[i].data) : NULL;
        if (!old) continue;
        rows[i].hasDelta=1;
        rows[i].memDelta=(NP_SSIZE)(rows[i].data.workingKB-old->data.workingKB);
        rows[i].faultDelta=(LONG)(rows[i].data.faults-old->data.faults);
        if (!elapsed || sample.cpuStatus<0 || !sysInfo.dwNumberOfProcessors) continue;
        userDelta=timeDelta(rows[i].data.user,old->data.user);
        kernelDelta=timeDelta(rows[i].data.kernel,old->data.kernel);
        if (userDelta>=0.0 && kernelDelta>=0.0)
            rows[i].cpu=percentage(userDelta+kernelDelta,
                (double)elapsed*10000.0*(double)sysInfo.dwNumberOfProcessors);
    }
    oldRows=processRows; oldCount=processCount;
    processRows=rows; processCount=count;
    processTick=now; processBaseline=1;
    sample.processes=count; sample.totalsValid=1;
    sample.handlesValid=sample.handleStatus>=0;
    sortRows();
    populateList(haveSelection?&selected:NULL,oldRows,oldCount);
    if (oldRows) GlobalFree((HGLOBAL)oldRows);
    return;
malformed:
    sample.processStatus=(LONG)0xc000000dL;
    sample.totalsValid=0; processBaseline=0;
    updateProcessControls();
}
static const PROCESSROW *selectedRow(void)
{
    int selection;
    if (!processList || !sample.totalsValid) return NULL;
    selection=(int)SendMessageA(processList,LB_GETCURSEL,0,0);
    return selection>=0 && (DWORD)selection<processCount ? &processRows[selection] : NULL;
}
/* Target of End Process and Set Priority: a selected, unprotected row. */
static const PROCESSROW *actionRow(void)
{
    const PROCESSROW *p=activeTab==TAB_PROCESSES && !processBusy ? selectedRow() : NULL;
    return p && !protectedProcess(p) ? p : NULL;
}
static void updateProcessControls(void)
{
    const PROCESSROW *p=selectedRow();
    if (endButton) EnableWindow(endButton,p && !protectedProcess(p));
}
static void updateProcessMenu(void)
{
    const PROCESSROW *p=actionRow();
    UINT state=MF_BYCOMMAND|(p ? MF_ENABLED : MF_GRAYED);
    int i, level;
    level=p ? priorityLevel(p->data.priority) : -1;
    EnableMenuItem(processMenu,ID_ENDPROCESS,state);
    for (i=0;i<3;++i) {
        EnableMenuItem(processMenu,ID_PRIORITY+i,state);
        CheckMenuItem(processMenu,ID_PRIORITY+i,MF_BYCOMMAND|(i==level ? MF_CHECKED : MF_UNCHECKED));
    }
}
static void layoutProcessControls(void)
{
    RECT r;
    int i,x,w,right;
    if (!processList) return;
    GetClientRect(mainWindow,&r);
    right=r.right-14-GetSystemMetrics(SM_CXVSCROLL);
    w=right-12;
    for (i=1;i<COLUMNS;++i) if (columnShown[i]) w-=columnWidths[i];
    columnWidths[0]=w<55 ? 55 : w;
    /* Headers past the list edge are cut or hidden; widen the window to see them. */
    for (i=0,x=12;i<COLUMNS;++i) {
        w=columnShown[i] && x<right ? columnWidths[i] : 0;
        if (x+w>right) w=right-x;
        ShowWindow(headers[i],activeTab==TAB_PROCESSES && w>0 ? SW_SHOW : SW_HIDE);
        if (w>0) MoveWindow(headers[i],x,42,w,22,TRUE);
        if (columnShown[i]) x+=columnWidths[i];
    }
    MoveWindow(processList,12,64,r.right-24,r.bottom-132,TRUE);
    MoveWindow(endButton,r.right-125,r.bottom-58,113,25,TRUE);
}
static void setTab(int tab)
{
    int i;
    activeTab=tab;
    ShowWindow(processList,tab==TAB_PROCESSES?SW_SHOW:SW_HIDE);
    ShowWindow(endButton,tab==TAB_PROCESSES?SW_SHOW:SW_HIDE);
    layoutProcessControls();
    showAppControls(tab==TAB_APPS);
    InvalidateRect(mainWindow,NULL,FALSE);
    for (i=0;i<TABS;++i) InvalidateRect(tabs[i],NULL,TRUE);
    SetFocus(tab==TAB_APPS ? appList : tab==TAB_PROCESSES ? processList : tabs[tab]);
}
static void drawProcessItem(const DRAWITEMSTRUCT *d)
{
    PROCESSROW *p;
    RECT cell, focus;
    HFONT old;
    COLORREF bg,fg;
    char buffer[300];
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
    for(i=0;i<COLUMNS;++i) {
        if(!columnShown[i]) continue;
        cellText(p,i,buffer);
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
    int tab;
    tab=(int)d->CtlID-ID_TAB;
    if (tab<0 || tab>=TABS) return;
    r=d->rcItem;
    brush=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    FillRect(d->hDC,&r,brush); DeleteObject(brush);
    FrameRect(d->hDC,&r,(HBRUSH)GetStockObject(tab==activeTab?WHITE_BRUSH:GRAY_BRUSH));
    old=(HFONT)SelectObject(d->hDC,uiFont);
    SetBkMode(d->hDC,TRANSPARENT); SetTextColor(d->hDC,GetSysColor(COLOR_BTNTEXT));
    DrawTextA(d->hDC,tabNames[tab],-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
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
    else { sortColumn=column; sortDescending=column>COL_PID; }
    sortRows(); populateList(valid?&selected:NULL,NULL,0);
}
static void toggleColumn(int column)
{
    columnShown[column]=!columnShown[column];
    layoutProcessControls();
    if (!columnShown[column] && column==sortColumn) sortByColumn(COL_NAME);
    else InvalidateRect(processList,NULL,TRUE);
}
/* Opens the selected process and checks its creation time, so a reused PID
 * never receives the action. */
static HANDLE openTarget(const PROCESSROW *target, DWORD access, const char *title)
{
    HANDLE handle;
    FILETIME created,exited,kernel,user;
    char prompt[600];
    DWORD error;
    handle=OpenProcess(access|PROCESS_QUERY_INFORMATION,FALSE,target->data.pid);
    if (!handle) {
        error=GetLastError();
        wsprintfA(prompt,"Cannot open %s (PID %lu).\nWindows error %lu.",target->name,(DWORD)target->data.pid,error);
        MessageBoxA(mainWindow,prompt,title,MB_OK|MB_ICONEXCLAMATION);
        return NULL;
    }
    if(!GetProcessTimes(handle,&created,&exited,&kernel,&user)) {
        error=GetLastError();
        wsprintfA(prompt,"Cannot verify the selected process.\nWindows error %lu. Nothing was changed.",error);
        MessageBoxA(mainWindow,prompt,title,MB_OK|MB_ICONEXCLAMATION);
    } else if(created.dwLowDateTime!=target->data.created.low ||
              created.dwHighDateTime!=target->data.created.high) {
        MessageBoxA(mainWindow,"The selected process has exited and its PID was reused.\nNothing was changed.",title,MB_OK|MB_ICONEXCLAMATION);
    } else return handle;
    CloseHandle(handle);
    return NULL;
}
static void endSelectedProcess(void)
{
    PROCESSROW target;
    const PROCESSROW *p=actionRow();
    HANDLE handle;
    char prompt[600];
    DWORD error;
    if (!p) return;
    /* Snapshot selection and hold an object handle across the modal dialog. */
    target=*p; processBusy=1;
    handle=openTarget(&target,PROCESS_TERMINATE,"End Process");
    if (handle) {
        wsprintfA(prompt,"End %s (PID %lu)?\n\nThe process will be terminated immediately.\nUnsaved work in that process will be lost.",target.name,(DWORD)target.data.pid);
        if(MessageBoxA(mainWindow,prompt,"End Process",MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2)==IDYES &&
           !TerminateProcess(handle,1)) {
            error=GetLastError();
            wsprintfA(prompt,"The process could not be terminated.\nWindows error %lu.",error);
            MessageBoxA(mainWindow,prompt,"End Process",MB_OK|MB_ICONEXCLAMATION);
        }
        CloseHandle(handle);
    }
    processBusy=0;
    takeSample();
}
static void setSelectedPriority(int level)
{
    static const DWORD classes[3]={IDLE_PRIORITY_CLASS,NORMAL_PRIORITY_CLASS,HIGH_PRIORITY_CLASS};
    PROCESSROW target;
    const PROCESSROW *p=actionRow();
    HANDLE handle;
    char prompt[600];
    DWORD error;
    if (!p) return;
    target=*p; processBusy=1;
    handle=openTarget(&target,PROCESS_SET_INFORMATION,"Set Priority");
    if (handle && !SetPriorityClass(handle,classes[level])) {
        error=GetLastError();
        wsprintfA(prompt,"Cannot change the priority of %s (PID %lu).\nWindows error %lu.",target.name,(DWORD)target.data.pid,error);
        MessageBoxA(mainWindow,prompt,"Set Priority",MB_OK|MB_ICONEXCLAMATION);
    }
    if (handle) CloseHandle(handle);
    processBusy=0;
    takeSample();
}
/* NT 3.1 has neither WM_CONTEXTMENU nor LB_ITEMFROMPOINT; rows are fixed height. */
static void processContextMenu(LPARAM position)
{
    POINT pt;
    int item;
    pt.x=(short)LOWORD(position); pt.y=(short)HIWORD(position);
    item=(int)SendMessageA(processList,LB_GETTOPINDEX,0,0)+pt.y/rowHeight;
    if (pt.y<0 || item>=(int)processCount) return;
    SendMessageA(processList,LB_SETCURSEL,(WPARAM)item,0);
    updateProcessControls();
    ClientToScreen(processList,&pt);
    TrackPopupMenu(processMenu,TPM_LEFTALIGN|TPM_RIGHTBUTTON,pt.x,pt.y,0,mainWindow,NULL);
}
static void createProcessControls(void)
{
    int i;
    HDC dc;
    TEXTMETRICA tm;
    HFONT old;
    for(i=0;i<TABS;++i) tabs[i]=CreateWindowA("BUTTON",tabNames[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
        10+i*90,8,i==TAB_PERFORMANCE?100:90,26,mainWindow,(HMENU)(WPARAM)(ID_TAB+i),instance,NULL);
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
