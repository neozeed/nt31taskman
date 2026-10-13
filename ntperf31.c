/* NTPERF R4 - shared UI for NT 3.1 x86 and modern Win64.
 * Plain USER/GDI, no CRT or common controls. PSAPI is Win64-only.
 * See README.md for the historical structure references and test limits.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define HISTORY 180
#define ID_EXIT 100
#define ID_TOP 101
#define ID_FAST 102
#define ID_NORMAL 103
#define ID_SLOW 104
#define ID_PAUSE 105
#define ID_KERNEL 106
#define ID_REFRESH 107
#define ID_COPY 108
#define ID_ABOUT 109
#define ID_RUN 110
#define ID_PERCPU 111
#define BAD (-1)
#include "backend.h"

static HINSTANCE instance;
static HWND mainWindow;
static HFONT uiFont, meterFont;
static SYSTEM_INFO sysInfo;
static SAMPLE sample;
static CPUINFO previous[CPU_MAX];
static int previousValid, previousCount;
static int cpuHistory[HISTORY], kernelHistory[HISTORY];
static int perCpuHistory[CPU_MAX][HISTORY], perKernelHistory[CPU_MAX][HISTORY];
static int perCpu;
static NP_SIZE memHistory[HISTORY];
static int memValid[HISTORY];
static int nextSample, historyCount;
static DWORD sampleSerial;
static UINT interval = 1000;
static int paused, topmost, showKernel;
static DWORD version;

static void clearBytes(void *p, DWORD n)
{
    BYTE *b;
    b = (BYTE *)p;
    while (n--) *b++ = 0;
}

#include "process-ui.inc"

static void sampleTotals(void)
{
    PROCDATA *rows;
    DWORD count;
    sample.totalsValid=0;
    sample.processStatus=backendReadProcesses(&rows,&count,&sample.processQueryStatus);
    if (sample.processStatus>=0) {
        collectProcesses(rows,count);
        GlobalFree((HGLOBAL)rows);
    } else { processBaseline=0; updateProcessControls(); }
}

static void takeSample(void)
{
    static CPUINFO current[CPU_MAX];
    DWORD i, count;

    for (i = 0; i < CPU_MAX; ++i) {
        perCpuHistory[i][nextSample] = BAD;
        perKernelHistory[i][nextSample] = BAD;
    }
    sample.cpu = sample.kernel = BAD;
    count = sysInfo.dwNumberOfProcessors;
    if (!count || count > CPU_MAX) count = CPU_MAX;
    sample.cpuStatus = backendReadCpu(current,count);
    if (sample.cpuStatus >= 0) {
        if (previousValid && previousCount == (int)count) {
            cpuPercentages(current, previous, count, &sample.cpu, &sample.kernel);
            for (i = 0; i < count; ++i)
                cpuPercentages(current+i, previous+i, 1,
                    &perCpuHistory[i][nextSample], &perKernelHistory[i][nextSample]);
        }
        for (i = 0; i < count; ++i) previous[i] = current[i];
        previousValid = 1;
        previousCount = (int)count;
    } else previousValid = 0;

    backendReadMemory(&sample,&sysInfo);
    sampleTotals();
    cpuHistory[nextSample] = sample.cpu;
    kernelHistory[nextSample] = sample.kernel;
    memHistory[nextSample] = sample.commitKB;
    memValid[nextSample] = sample.memoryValid && sample.limitKB > 0;
    nextSample = (nextSample + 1) % HISTORY;
    if (historyCount < HISTORY) ++historyCount;
    ++sampleSerial;
    InvalidateRect(mainWindow, NULL, FALSE);
}

static void rect(HDC dc, int x, int y, int w, int h, COLORREF color)
{
    RECT r;
    HBRUSH brush;
    if (w <= 0 || h <= 0) return;
    SetRect(&r, x, y, x+w, y+h);
    brush = CreateSolidBrush(color);
    FillRect(dc, &r, brush);
    DeleteObject(brush);
}

static void line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color)
{
    HPEN pen, old;
    pen = CreatePen(PS_SOLID, 1, color);
    old = (HPEN)SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

static void text(HDC dc, int x, int y, const char *value)
{
    TextOutA(dc, x, y, value, lstrlenA(value));
}

static void group(HDC dc, int x, int y, int w, int h, const char *title)
{
    SIZE size;
    COLORREF face;
    face = GetSysColor(COLOR_BTNFACE);
    line(dc,x,y+7,x+w,y+7,GetSysColor(COLOR_BTNSHADOW));
    line(dc,x,y+7,x,y+h,GetSysColor(COLOR_BTNSHADOW));
    line(dc,x+1,y+h,x+w,y+h,GetSysColor(COLOR_BTNHIGHLIGHT));
    line(dc,x+w,y+7,x+w,y+h+1,GetSysColor(COLOR_BTNHIGHLIGHT));
    GetTextExtentPointA(dc,title,lstrlenA(title),&size);
    rect(dc,x+8,y,size.cx+6,size.cy,face);
    text(dc,x+11,y,title);
}

static void inset(HDC dc, int x, int y, int w, int h)
{
    line(dc,x-1,y-1,x+w,y-1,GetSysColor(COLOR_BTNSHADOW));
    line(dc,x-1,y-1,x-1,y+h,GetSysColor(COLOR_BTNSHADOW));
    line(dc,x-1,y+h,x+w+1,y+h,GetSysColor(COLOR_BTNHIGHLIGHT));
    line(dc,x+w,y-1,x+w,y+h,GetSysColor(COLOR_BTNHIGHLIGHT));
    rect(dc,x,y,w,h,RGB(0,0,0));
}

static void graphTrace(HDC dc, int x, int y, int w, int h, int type, int processor, COLORREF color)
{
    int i, index, value, px, py, drawing;
    HPEN pen, old;
    pen = CreatePen(PS_SOLID,1,color);
    old = (HPEN)SelectObject(dc,pen);
    drawing = 0;
    for (i = 0; i < historyCount; ++i) {
        index = (nextSample - historyCount + i + HISTORY) % HISTORY;
        if (type == 0) value = processor < 0 ? cpuHistory[index] : perCpuHistory[processor][index];
        else if (type == 1) value = processor < 0 ? kernelHistory[index] : perKernelHistory[processor][index];
        else value = memValid[index] && sample.memoryValid ?
            percentage((double)memHistory[index],(double)sample.limitKB) : BAD;
        if (value == BAD) { drawing = 0; continue; }
        px = x + (HISTORY-historyCount+i) * (w-1) / (HISTORY-1);
        py = y + h-1 - value * (h-1) / 100;
        if (!drawing) MoveToEx(dc,px,py,NULL);
        else LineTo(dc,px,py);
        SetPixel(dc,px,py,color);
        drawing = 1;
    }
    SelectObject(dc,old);
    DeleteObject(pen);
}

static void graph(HDC dc, int x, int y, int w, int h, int memory, int processor)
{
    int i, offset, step;
    if (w < 2 || h < 2) return;
    inset(dc,x,y,w,h);
    step = 12;
    offset = (int)((sampleSerial * (DWORD)(w-1) / (HISTORY-1)) % step);
    for (i=x+w-1-offset; i>=x; i-=step)
        line(dc,i,y,i,y+h,RGB(0,70,0));
    for (i=y+h-1; i>=y; i-=12)
        line(dc,x,i,x+w,i,RGB(0,70,0));
    graphTrace(dc,x,y,w,h,memory ? 2 : 0,processor,RGB(0,255,0));
    if (!memory && showKernel) graphTrace(dc,x,y,w,h,1,processor,RGB(255,64,64));
}

/* Tile in processor-number order, left to right then top to bottom. */
static void cpuGraphs(HDC dc, int x, int y, int w, int h)
{
    int count, cols, rows, i, left, top, right, bottom;
    char label[24];
    count = (int)sysInfo.dwNumberOfProcessors;
    if (count < 1 || count > CPU_MAX) count = CPU_MAX;
    if (!perCpu || count == 1) { graph(dc,x,y,w,h,0,-1); return; }
    cols = count;
    if (cols > 8) cols = 8;
    while (cols > 1 && w / cols < 28) --cols;
    rows = (count+cols-1)/cols;
    for (i=0; i<count; ++i) {
        left = x+(i%cols)*w/cols;
        right = x+(i%cols+1)*w/cols-5;
        top = y+(i/cols)*h/rows;
        bottom = y+(i/cols+1)*h/rows-4;
        graph(dc,left,top,right-left,bottom-top,0,i);
        wsprintfA(label,"%d",i);
        SetTextColor(dc,RGB(180,220,180));
        if (bottom-top>=14 && right-left>=18) text(dc,left+3,top+1,label);
    }
    SetTextColor(dc,GetSysColor(COLOR_BTNTEXT));
}

static void meter(HDC dc, int x, int y, int w, int h, int memory)
{
    int level, rows, row, cy, barWidth;
    RECT r;
    char label[48];
    HFONT old;
    inset(dc,x,y,w,h);
    level = memory ? (sample.memoryValid ?
        percentage((double)sample.commitKB,(double)sample.limitKB) : BAD) : sample.cpu;
    rows = (h-28)/4;
    if (rows < 1) rows = 1;
    barWidth = w / 4;
    for (row=0; row<rows; ++row) {
        cy = y+h-27-row*4;
        rect(dc,x+w/2-barWidth-2,cy,barWidth,2,
            level >= 0 && row*100/rows < level ? RGB(0,255,0) : RGB(0,62,0));
        rect(dc,x+w/2+2,cy,barWidth,2,
            level >= 0 && row*100/rows < level ? RGB(0,255,0) : RGB(0,62,0));
    }
    if (level < 0) lstrcpyA(label,"--");
    else if (memory) { formatSize(label,sample.commitKB); lstrcatA(label," K"); }
    else wsprintfA(label,"%d%%",sample.cpu);
    old = (HFONT)SelectObject(dc,meterFont);
    SetTextColor(dc,RGB(0,255,0));
    SetRect(&r,x,y+h-23,x+w,y+h-2);
    DrawTextA(dc,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(dc,old);
    SetTextColor(dc,GetSysColor(COLOR_BTNTEXT));
}

static void numberRow(HDC dc, int x, int y, int w, const char *label, NP_SIZE n, int valid)
{
    char buffer[40];
    RECT r;
    text(dc,x,y,label);
    if (valid) formatSize(buffer,n);
    else lstrcpyA(buffer,"--");
    SetRect(&r,x,y,x+w,y+20);
    DrawTextA(dc,buffer,-1,&r,DT_RIGHT|DT_SINGLELINE);
}

static void paintContents(HDC dc, int w, int h)
{
    int gap, meterWidth, graphX, graphWidth, rowH, lowerY, columnWidth, boxH;
    NP_SIZE used;
    char buffer[180];
    RECT status;
    HFONT old;
    old = (HFONT)SelectObject(dc,uiFont);
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,GetSysColor(COLOR_BTNTEXT));
    rect(dc,0,0,w,h,GetSysColor(COLOR_BTNFACE));
    gap = 12; meterWidth = 108;
    graphX = gap+meterWidth+gap;
    graphWidth = w-graphX-gap;
    rowH = (h-232)/2;
    if (rowH < 62) rowH = 62;
    group(dc,gap,8,meterWidth,rowH,"CPU Usage");
    group(dc,graphX,8,graphWidth,rowH,"CPU Usage History");
    meter(dc,gap+9,31,meterWidth-18,rowH-34,0);
    cpuGraphs(dc,graphX+9,31,graphWidth-18,rowH-34);
    group(dc,gap,rowH+16,meterWidth,rowH,"MEM Usage");
    group(dc,graphX,rowH+16,graphWidth,rowH,"Memory Usage History");
    meter(dc,gap+9,rowH+39,meterWidth-18,rowH-34,1);
    graph(dc,graphX+9,rowH+39,graphWidth-18,rowH-34,1,-1);
    lowerY = rowH*2+32;
    boxH = 78;
    columnWidth = (w-3*gap)/2;
    group(dc,gap,lowerY,columnWidth,boxH,"Totals");
    numberRow(dc,gap+10,lowerY+21,columnWidth-20,"Handles",sample.handles,sample.handlesValid);
    numberRow(dc,gap+10,lowerY+38,columnWidth-20,"Threads",sample.threads,sample.totalsValid);
    numberRow(dc,gap+10,lowerY+55,columnWidth-20,"Processes",sample.processes,sample.totalsValid);
    group(dc,columnWidth+2*gap,lowerY,columnWidth,boxH,"Physical Memory (K)");
    used = sample.totalKB >= sample.availableKB ? sample.totalKB-sample.availableKB : 0;
    numberRow(dc,columnWidth+2*gap+10,lowerY+21,columnWidth-20,"Total",sample.totalKB,sample.memoryValid);
    numberRow(dc,columnWidth+2*gap+10,lowerY+38,columnWidth-20,"Available",sample.availableKB,sample.memoryValid);
    numberRow(dc,columnWidth+2*gap+10,lowerY+55,columnWidth-20,"Used",used,sample.memoryValid);
    lowerY += boxH+7;
    group(dc,gap,lowerY,columnWidth,boxH,"Commit Charge (K)");
    numberRow(dc,gap+10,lowerY+21,columnWidth-20,"Total",sample.commitKB,sample.memoryValid);
    numberRow(dc,gap+10,lowerY+38,columnWidth-20,"Limit",sample.limitKB,sample.memoryValid);
    numberRow(dc,gap+10,lowerY+55,columnWidth-20,"Peak",sample.peakKB,sample.memoryValid);
    group(dc,columnWidth+2*gap,lowerY,columnWidth,boxH,"Kernel Pool Memory (K)");
    numberRow(dc,columnWidth+2*gap+10,lowerY+21,columnWidth-20,"Total",sample.pagedKB+sample.nonpagedKB,sample.memoryValid);
    numberRow(dc,columnWidth+2*gap+10,lowerY+38,columnWidth-20,"Paged",sample.pagedKB,sample.memoryValid);
    numberRow(dc,columnWidth+2*gap+10,lowerY+55,columnWidth-20,"Nonpaged",sample.nonpagedKB,sample.memoryValid);
    line(dc,0,h-23,w,h-23,GetSysColor(COLOR_BTNSHADOW));
    if (sample.cpuStatus < 0 || sample.perfStatus < 0)
        lstrcpyA(buffer,"Statistics unavailable - Help > Copy diagnostics");
    else if (paused) lstrcpyA(buffer,"Paused - View > Paused to resume");
    else wsprintfA(buffer,"%lu CPU(s)   |   %lu ms / sample   |   History: %lu samples",
        sysInfo.dwNumberOfProcessors,(DWORD)interval,(DWORD)HISTORY);
    SetRect(&status,7,h-20,w-7,h);
    DrawTextA(dc,buffer,-1,&status,DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    SelectObject(dc,old);
}

static void paintFrame(HDC dc, int w, int h)
{
    char buffer[200], cpu[16], commit[24], limit[24];
    HFONT old;
    RECT r;
    rect(dc,0,0,w,h,GetSysColor(COLOR_BTNFACE));
    if (activeTab==1) {
        SetViewportOrgEx(dc,0,34,NULL);
        paintContents(dc,w,h-34);
        SetViewportOrgEx(dc,0,0,NULL);
        return;
    }
    old=(HFONT)SelectObject(dc,uiFont);
    SetTextColor(dc,GetSysColor(COLOR_BTNTEXT)); SetBkMode(dc,TRANSPARENT);
    line(dc,0,h-23,w,h-23,GetSysColor(COLOR_BTNSHADOW));
    if (!sample.totalsValid) lstrcpyA(buffer,"Process query failed; displayed rows are stale. See Copy diagnostics.");
    else if (paused) lstrcpyA(buffer,"Paused - F5 refreshes the list; Space on the background resumes");
    else {
        if(sample.cpu<0) lstrcpyA(cpu,"--"); else wsprintfA(cpu,"%d%%",sample.cpu);
        formatSize(commit,sample.commitKB); formatSize(limit,sample.limitKB);
        if(sample.memoryValid) wsprintfA(buffer,"Processes: %lu   |   CPU: %s   |   Commit: %s K / %s K",
            sample.processes,cpu,commit,limit);
        else wsprintfA(buffer,"Processes: %lu   |   CPU: %s   |   Memory unavailable",sample.processes,cpu);
    }
    SetRect(&r,7,h-21,w-7,h);
    DrawTextA(dc,buffer,-1,&r,DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    SelectObject(dc,old);
}

static void copyDiagnostics(HWND hwnd)
{
    char buffer[1600], values[7][24];
    int i;
    NP_SIZE memoryValues[7];
    HGLOBAL block;
    char *destination;
    DWORD length;
    memoryValues[0]=sample.totalKB; memoryValues[1]=sample.availableKB;
    memoryValues[2]=sample.commitKB; memoryValues[3]=sample.limitKB;
    memoryValues[4]=sample.peakKB; memoryValues[5]=sample.pagedKB;
    memoryValues[6]=sample.nonpagedKB;
    for (i=0;i<7;++i) formatSize(values[i],memoryValues[i]);
    wsprintfA(buffer,
        "NTPERF R4 (%s)\r\nWindows version: %lu.%lu build %lu\r\n"
        "Processors: %lu; page size: %lu\r\n"
        "Sampling status: CPU=%08lX Performance=%08lX Processes=%08lX Handles=%08lX\r\n"
        "Process query=%08lX (Processes above includes decoding); memory Win32 error=%lu\r\n"
        "CPU=%d%% Kernel=%d%% MemoryValid=%d TotalsValid=%d HandlesValid=%d\r\n"
        "Physical=%sK Available=%sK Commit=%sK Limit=%sK Peak=%sK\r\n"
        "Paged pool=%sK Nonpaged pool=%sK Processes=%lu Threads=%lu Handles=%lu\r\n",
        backendName(),version&255,(version>>8)&255,(version>>16)&65535,
        sysInfo.dwNumberOfProcessors,sysInfo.dwPageSize,
        (DWORD)sample.cpuStatus,(DWORD)sample.perfStatus,(DWORD)sample.processStatus,(DWORD)sample.handleStatus,
        (DWORD)sample.processQueryStatus,sample.memoryError,
        sample.cpu,sample.kernel,sample.memoryValid,sample.totalsValid,sample.handlesValid,
        values[0],values[1],values[2],values[3],values[4],values[5],values[6],sample.processes,sample.threads,sample.handles);
    length = (DWORD)lstrlenA(buffer)+1;
    block = GlobalAlloc(GMEM_MOVEABLE,length);
    if (!block) return;
    destination = (char *)GlobalLock(block);
    if (!destination) { GlobalFree(block); return; }
    lstrcpyA(destination,buffer);
    GlobalUnlock(block);
    if (!OpenClipboard(hwnd)) { GlobalFree(block); return; }
    EmptyClipboard();
    if (!SetClipboardData(CF_TEXT,block)) GlobalFree(block);
    CloseClipboard();
}

static void updateMenu(HWND hwnd)
{
    HMENU menu;
    menu = GetMenu(hwnd);
    CheckMenuItem(menu,ID_PERCPU,MF_BYCOMMAND|(perCpu?MF_CHECKED:MF_UNCHECKED));
    CheckMenuItem(menu,ID_TOP,MF_BYCOMMAND|(topmost?MF_CHECKED:MF_UNCHECKED));
    CheckMenuItem(menu,ID_KERNEL,MF_BYCOMMAND|(showKernel?MF_CHECKED:MF_UNCHECKED));
    CheckMenuItem(menu,ID_PAUSE,MF_BYCOMMAND|(paused?MF_CHECKED:MF_UNCHECKED));
    CheckMenuItem(menu,ID_FAST,MF_BYCOMMAND|(interval==500?MF_CHECKED:MF_UNCHECKED));
    CheckMenuItem(menu,ID_NORMAL,MF_BYCOMMAND|(interval==1000?MF_CHECKED:MF_UNCHECKED));
    CheckMenuItem(menu,ID_SLOW,MF_BYCOMMAND|(interval==2000?MF_CHECKED:MF_UNCHECKED));
}

#include "run-ui.inc"

static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    RECT area;
    HDC dc, back;
    HBITMAP bitmap, oldBitmap;
    MINMAXINFO *limits;
    int command;
    switch (message) {
    case WM_ERASEBKGND: return 1;
    case WM_GETMINMAXINFO:
        limits = (MINMAXINFO *)lp;
        SetRect(&area,0,0,480,390);
        AdjustWindowRect(&area,WS_OVERLAPPEDWINDOW,TRUE);
        limits->ptMinTrackSize.x = area.right-area.left;
        limits->ptMinTrackSize.y = area.bottom-area.top;
        return 0;
    case WM_SIZE: layoutProcessControls(); InvalidateRect(hwnd,NULL,FALSE); return 0;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT *)lp)->itemHeight=(UINT)rowHeight;
        return TRUE;
    case WM_DRAWITEM:
        if (((DRAWITEMSTRUCT *)lp)->CtlID==ID_LIST) drawProcessItem((DRAWITEMSTRUCT *)lp);
        else drawTab((DRAWITEMSTRUCT *)lp);
        return TRUE;
    case WM_TIMER: if (!paused && !endingProcess) takeSample(); return 0;
    case WM_KEYDOWN:
        if (wp == VK_F5) takeSample();
        if (wp == VK_SPACE) SendMessageA(hwnd,WM_COMMAND,ID_PAUSE,0);
        return 0;
    case WM_COMMAND:
        command = LOWORD(wp);
        if (command == ID_PROCESSES) setProcessTab(0);
        else if (command == ID_PERFORMANCE) setProcessTab(1);
        else if (command == ID_LIST) updateProcessControls();
        else if (command == ID_ENDPROCESS) endSelectedProcess();
        else if (command >= ID_COLUMN && command < ID_COLUMN+COLUMNS) sortByColumn(command-ID_COLUMN);
        else if (command == ID_RUN) showRunDialog(hwnd);
        else if (command == ID_PERCPU) perCpu = !perCpu;
        else if (command == ID_EXIT) DestroyWindow(hwnd);
        else if (command == ID_TOP) {
            topmost = !topmost;
            SetWindowPos(hwnd,topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        } else if (command == ID_KERNEL) showKernel = !showKernel;
        else if (command == ID_PAUSE) {
            paused = !paused;
            previousValid = 0;
            processBaseline = 0;
            if (!paused) takeSample();
        } else if (command == ID_REFRESH) takeSample();
        else if (command == ID_COPY) copyDiagnostics(hwnd);
        else if (command == ID_ABOUT) MessageBoxA(hwnd,
            "NTPERF R4\nNT 4-style performance graphs for NT 3.1 and Win64.\n\n"
            "Green: total CPU / committed memory.\nRed (optional): busy kernel CPU.\n"
            "Memory history is scaled to the current commit limit.\n\n"
            "F5: sample now   Space: pause/resume\n"
            "Per-processor graphs: View > One graph per CPU.\nFile > Run starts a program.","About NTPERF31",MB_OK);
        else if (command == ID_FAST || command == ID_NORMAL || command == ID_SLOW) {
            interval = command==ID_FAST ? 500 : (command==ID_NORMAL ? 1000 : 2000);
            /* A constant sample interval makes the history time axis honest. */
            nextSample = historyCount = 0;
            sampleSerial = 0;
            previousValid = 0;
            processBaseline = 0;
            KillTimer(hwnd,1);
            SetTimer(hwnd,1,interval,NULL);
            if (!paused) takeSample();
        }
        if (command != ID_EXIT) {
            updateMenu(hwnd);
            InvalidateRect(hwnd,NULL,FALSE);
        }
        return 0;
    case WM_PAINT:
        dc = BeginPaint(hwnd,&ps);
        GetClientRect(hwnd,&area);
        if (area.right > 0 && area.bottom > 0) {
            back = CreateCompatibleDC(dc);
            bitmap = CreateCompatibleBitmap(dc,area.right,area.bottom);
            if (back && bitmap) {
                oldBitmap = (HBITMAP)SelectObject(back,bitmap);
                paintFrame(back,area.right,area.bottom);
                BitBlt(dc,0,0,area.right,area.bottom,back,0,0,SRCCOPY);
                SelectObject(back,oldBitmap);
            } else paintFrame(dc,area.right,area.bottom);
            if (bitmap) DeleteObject(bitmap);
            if (back) DeleteDC(back);
        }
        EndPaint(hwnd,&ps);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd,1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd,message,wp,lp);
}

static HMENU createMenu(void)
{
    HMENU menu, file, options, view, help;
    menu=CreateMenu(); file=CreatePopupMenu(); options=CreatePopupMenu();
    view=CreatePopupMenu(); help=CreatePopupMenu();
    AppendMenuA(file,MF_STRING,ID_RUN,"&Run...");
    AppendMenuA(file,MF_SEPARATOR,0,NULL);
    AppendMenuA(file,MF_STRING,ID_EXIT,"E&xit");
    AppendMenuA(options,MF_STRING,ID_TOP,"Always on &top");
    AppendMenuA(view,MF_STRING,ID_FAST,"&High speed (0.5 seconds)");
    AppendMenuA(view,MF_STRING|MF_CHECKED,ID_NORMAL,"&Normal speed (1 second)");
    AppendMenuA(view,MF_STRING,ID_SLOW,"&Low speed (2 seconds)");
    AppendMenuA(view,MF_SEPARATOR,0,NULL);
    AppendMenuA(view,MF_STRING,ID_PAUSE,"&Paused\tSpace");
    AppendMenuA(view,MF_STRING,ID_REFRESH,"&Refresh now\tF5");
    AppendMenuA(view,MF_STRING,ID_PERCPU,"One graph per &CPU");
    AppendMenuA(view,MF_STRING,ID_KERNEL,"Show &kernel times");
    AppendMenuA(help,MF_STRING,ID_COPY,"&Copy diagnostics");
    AppendMenuA(help,MF_STRING,ID_ABOUT,"&About NTPERF31");
    AppendMenuA(menu,MF_POPUP,(WPARAM)file,"&File");
    AppendMenuA(menu,MF_POPUP,(WPARAM)options,"&Options");
    AppendMenuA(menu,MF_POPUP,(WPARAM)view,"&View");
    AppendMenuA(menu,MF_POPUP,(WPARAM)help,"&Help");
    return menu;
}

static void runApp(void)
{
    WNDCLASSA wc;
    MSG msg;
    HDC dc;
    int dpi, result;
    RECT bounds;
    instance = GetModuleHandleA(NULL);
    if (!backendInit(&version)) {
        MessageBoxA(NULL,"This program requires Windows NT and NtQuerySystemInformation.","NTPERF31",MB_OK|MB_ICONSTOP);
        ExitProcess(1);
    }
    GetSystemInfo(&sysInfo);
    perCpu = sysInfo.dwNumberOfProcessors > 1;
    dc = GetDC(NULL);
    dpi = GetDeviceCaps(dc,LOGPIXELSY);
    ReleaseDC(NULL,dc);
    uiFont = CreateFontA(-MulDiv(8,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
        VARIABLE_PITCH|FF_SWISS,"MS Sans Serif");
    meterFont = CreateFontA(-MulDiv(10,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
        FIXED_PITCH|FF_MODERN,"Courier New");
    clearBytes(&wc,sizeof(wc));
    wc.style = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconA(NULL,IDI_APPLICATION);
    wc.hCursor = LoadCursorA(NULL,IDC_ARROW);
    wc.lpszClassName = "NTPerf31Window";
    if (!RegisterClassA(&wc)) ExitProcess(2);
    SetRect(&bounds,0,0,620,434);
    AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,TRUE);
    mainWindow = CreateWindowA(wc.lpszClassName,APP_TITLE,
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,
        bounds.right-bounds.left,bounds.bottom-bounds.top,
        NULL,createMenu(),instance,NULL);
    if (!mainWindow) ExitProcess(3);
    updateMenu(mainWindow);
    createProcessControls();
    takeSample();
    ShowWindow(mainWindow,SW_SHOWNORMAL);
    UpdateWindow(mainWindow);
    SetTimer(mainWindow,1,interval,NULL);
    while ((result=GetMessageA(&msg,NULL,0,0)) > 0) {
        if (msg.message==WM_KEYDOWN && msg.wParam==VK_F5) { takeSample(); continue; }
        if (msg.message==WM_KEYDOWN && msg.wParam==VK_TAB && (GetKeyState(VK_CONTROL)&0x8000)) {
            setProcessTab(!activeTab); continue;
        }
        if (msg.message==WM_KEYDOWN && msg.wParam==VK_DELETE && msg.hwnd==processList) {
            endSelectedProcess(); continue;
        }
        if (IsDialogMessageA(mainWindow,&msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (processRows) GlobalFree((HGLOBAL)processRows);
    backendShutdown();
    if (uiFont) DeleteObject(uiFont);
    if (meterFont) DeleteObject(meterFont);
    ExitProcess(result < 0 ? 4 : 0);
}

#ifdef NTPERF_USE_CRT
int WINAPI WinMain(HINSTANCE current, HINSTANCE previousInstance,
                   LPSTR commandLine, int show)
{
    (void)current;
    (void)previousInstance;
    (void)commandLine;
    (void)show;
    runApp();
    return 0;
}
#else
void WinMainCRTStartup(void)
{
    runApp();
}
#endif
