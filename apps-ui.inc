/* Included by ntperf31.c: Applications page. EndTask, SwitchToThisWindow and
 * InternalGetWindowText are unlisted USER32 exports on NT 3.1 and 3.5. */
#define ID_APPLIST 250
#define ID_ENDTASK 251
#define ID_SWITCHTO 252
#define WM_ENDTASKDONE (WM_USER+100)
#define MAX_APPS 256
typedef BOOL (WINAPI *END_TASK)(HWND, BOOL, BOOL);
typedef void (WINAPI *SWITCH_TO)(HWND, BOOL);
typedef int (WINAPI *WINDOW_TEXT)(HWND, LPWSTR, int);
typedef struct { HWND hwnd; char title[160]; } APPROW;
static HWND appList, endTaskButton, switchButton, endingTask;
static DWORD endingPid, appCount, foundCount;
static APPROW appRows[MAX_APPS], foundApps[MAX_APPS];
static END_TASK endTask;
static SWITCH_TO switchTo;
static WINDOW_TEXT windowText;

/* InternalGetWindowText does not send WM_GETTEXT, so a hung application
 * cannot stall the list. */
static void windowTitle(HWND hwnd, char *title, int size)
{
    WCHAR wide[160];
    int n;
    title[0]=0;
    if (!windowText) { GetWindowTextA(hwnd,title,size); return; }
    n=windowText(hwnd,wide,160);
    if (n<=0) return;
    n=WideCharToMultiByte(CP_ACP,0,wide,n<160?n:159,title,size-1,NULL,NULL);
    title[n>0?n:0]=0;
}
/* Same filter as the NT 3.x Task List: visible, unowned, titled, not ours. */
static BOOL CALLBACK collectApp(HWND hwnd, LPARAM unused)
{
    APPROW *app;
    DWORD pid;
    (void)unused;
    if (foundCount>=MAX_APPS) return FALSE;
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd,GW_OWNER)) return TRUE;
    GetWindowThreadProcessId(hwnd,&pid);
    if (pid==GetCurrentProcessId()) return TRUE;
    app=&foundApps[foundCount];
    windowTitle(hwnd,app->title,(int)sizeof(app->title));
    if (!app->title[0]) return TRUE;
    app->hwnd=hwnd;
    ++foundCount;
    return TRUE;
}
static int appBefore(const APPROW *a, const APPROW *b)
{
    int order=lstrcmpiA(a->title,b->title);
    return order ? order<0 : a->hwnd<b->hwnd;
}
static int sameApps(void)
{
    DWORD i;
    if (foundCount!=appCount) return 0;
    for (i=0;i<appCount;++i)
        if (foundApps[i].hwnd!=appRows[i].hwnd || lstrcmpA(foundApps[i].title,appRows[i].title)) return 0;
    return 1;
}
static HWND selectedApp(void)
{
    int selection;
    if (!appList) return NULL;
    selection=(int)SendMessageA(appList,LB_GETCURSEL,0,0);
    if (selection<0 || (DWORD)selection>=appCount || !IsWindow(appRows[selection].hwnd)) return NULL;
    return appRows[selection].hwnd;
}
static void updateAppControls(void)
{
    HWND app=selectedApp();
    if (!appList) return;
    EnableWindow(switchButton,app!=NULL);
    EnableWindow(endTaskButton,app!=NULL && !endingTask);
}
/* Sorted by title so z-order changes do not reshuffle the list; the
 * listbox is rebuilt only when a window or title changes. */
static void refreshApps(void)
{
    APPROW value;
    DWORD i, j;
    int selection, top;
    HWND selected;
    if (!appList) return;
    foundCount=0;
    EnumWindows(collectApp,0);
    for (i=1;i<foundCount;++i) {
        value=foundApps[i];
        for (j=i;j && appBefore(&value,&foundApps[j-1]);--j) foundApps[j]=foundApps[j-1];
        foundApps[j]=value;
    }
    if (sameApps()) { updateAppControls(); return; }
    selection=(int)SendMessageA(appList,LB_GETCURSEL,0,0);
    selected=selection>=0 && (DWORD)selection<appCount ? appRows[selection].hwnd : NULL;
    top=(int)SendMessageA(appList,LB_GETTOPINDEX,0,0);
    SendMessageA(appList,WM_SETREDRAW,FALSE,0);
    SendMessageA(appList,LB_RESETCONTENT,0,0);
    selection=-1;
    for (i=0;i<foundCount;++i) {
        appRows[i]=foundApps[i];
        SendMessageA(appList,LB_ADDSTRING,0,(LPARAM)appRows[i].title);
        if (appRows[i].hwnd==selected) selection=(int)i;
    }
    appCount=foundCount;
    SendMessageA(appList,LB_SETCURSEL,(WPARAM)selection,0);
    SendMessageA(appList,LB_SETTOPINDEX,(WPARAM)top,0);
    SendMessageA(appList,WM_SETREDRAW,TRUE,0);
    InvalidateRect(appList,NULL,TRUE);
    updateAppControls();
}
/* As the NT 3.x Task List: activate the last active popup, so a dialog
 * comes up together with its owner. */
static void switchToApp(HWND hwnd)
{
    HWND popup=GetLastActivePopup(hwnd);
    if (IsWindow(popup)) hwnd=popup;
    if (switchTo) { switchTo(hwnd,TRUE); return; }
    if (IsIconic(hwnd)) ShowWindow(hwnd,SW_RESTORE);
    SetForegroundWindow(hwnd);
}
static void switchToSelected(void)
{
    HWND app=selectedApp();
    if (app) switchToApp(app);
}
static DWORD WINAPI endTaskThread(LPVOID app)
{
    PostMessageA(mainWindow,WM_ENDTASKDONE,0,(LPARAM)endTask((HWND)app,FALSE,FALSE));
    return 0;
}
/* EndTask sends WM_QUERYENDSESSION/WM_ENDSESSION and may show USER's
 * not-responding dialog, so it blocks; the NT 3.x Task List runs it on a thread. */
static void endSelectedTask(void)
{
    HWND app=selectedApp();
    HANDLE thread;
    DWORD id;
    if (!app || endingTask) return;
    if (!endTask) { PostMessageA(app,WM_CLOSE,0,0); return; }
    switchToApp(app);
    GetWindowThreadProcessId(app,&endingPid);
    endingTask=app;
    thread=CreateThread(NULL,0,endTaskThread,(LPVOID)app,0,&id);
    if (!thread) { endingTask=NULL; PostMessageA(app,WM_CLOSE,0,0); return; }
    CloseHandle(thread);
    updateAppControls();
}
/* EndTask returned FALSE: the application refused (e.g. Cancel on its save
 * prompt). Offer to force it, unless the window is gone or now belongs to
 * another process. */
static void endTaskDone(BOOL closed)
{
    HWND app=endingTask;
    DWORD pid=0;
    char title[160], prompt[400];
    endingTask=NULL;
    refreshApps();
    if (closed || !IsWindow(app)) return;
    GetWindowThreadProcessId(app,&pid);
    if (pid!=endingPid) return;
    windowTitle(app,title,(int)sizeof(title));
    wsprintfA(prompt,"%s has chosen not to close. It may be completing a lengthy task.\n\n"
        "Forcing it to close may lose data. Force it to close?",title);
    SetForegroundWindow(mainWindow);
    if (MessageBoxA(mainWindow,prompt,"End Task",MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2)==IDYES)
        endTask(app,FALSE,TRUE);
}
static void appListNotify(int code)
{
    if (code==LBN_DBLCLK) switchToSelected();
    updateAppControls();
}
static void layoutAppControls(void)
{
    RECT r;
    if (!appList) return;
    GetClientRect(mainWindow,&r);
    MoveWindow(appList,12,42,r.right-24,r.bottom-110,TRUE);
    MoveWindow(endTaskButton,r.right-246,r.bottom-58,113,25,TRUE);
    MoveWindow(switchButton,r.right-125,r.bottom-58,113,25,TRUE);
}
static void showAppControls(int show)
{
    int command=show ? SW_SHOW : SW_HIDE;
    ShowWindow(appList,command);
    ShowWindow(endTaskButton,command);
    ShowWindow(switchButton,command);
    if (show) refreshApps();
}
static void createAppControls(void)
{
    union { FARPROC generic; END_TASK end; SWITCH_TO to; WINDOW_TEXT text; } fn;
    HMODULE user=GetModuleHandleA("user32.dll");
    fn.generic=GetProcAddress(user,"EndTask"); endTask=fn.end;
    fn.generic=GetProcAddress(user,"SwitchToThisWindow"); switchTo=fn.to;
    fn.generic=GetProcAddress(user,"InternalGetWindowText"); windowText=fn.text;
    appList=CreateWindowA("LISTBOX","",WS_CHILD|WS_TABSTOP|WS_BORDER|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,
        0,0,1,1,mainWindow,(HMENU)ID_APPLIST,instance,NULL);
    endTaskButton=CreateWindowA("BUTTON","&End Task",WS_CHILD|WS_TABSTOP|BS_PUSHBUTTON,
        0,0,1,1,mainWindow,(HMENU)ID_ENDTASK,instance,NULL);
    switchButton=CreateWindowA("BUTTON","&Switch To",WS_CHILD|WS_TABSTOP|BS_PUSHBUTTON,
        0,0,1,1,mainWindow,(HMENU)ID_SWITCHTO,instance,NULL);
    SendMessageA(appList,WM_SETFONT,(WPARAM)uiFont,FALSE);
    SendMessageA(endTaskButton,WM_SETFONT,(WPARAM)uiFont,FALSE);
    SendMessageA(switchButton,WM_SETFONT,(WPARAM)uiFont,FALSE);
    layoutAppControls(); updateAppControls();
}
