/* Included by ntperf3x.c: replaces Program Manager's Task List. NT 3.5's
 * Program Manager runs Winlogon\Taskman at logon instead of starting its own
 * Task List; NT 3.1's has no such setting. RegisterTasklist is an unlisted
 * USER32 export: USER then posts SC_TASKLIST (desktop double-click, Switch
 * To...) to this thread. */
#define WM_SHOWTASKLIST (WM_USER+101)
#define WINLOGON_KEY "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon"
typedef BOOL (WINAPI *REGISTER_TASKLIST)(HWND);
static int tasklistSupported(void)
{
    return LOBYTE(LOWORD(version))==3 && HIBYTE(LOWORD(version))>=50;
}
static void tasklistCommand(char *command)
{
    char path[MAX_PATH];
    DWORD n=GetModuleFileNameA(NULL,path,MAX_PATH-1);
    int i, space=0;
    path[n]=0;
    for (i=0;path[i];++i) if (path[i]==' ') space=1;
    wsprintfA(command,space?"\"%s\" /tasklist":"%s /tasklist",path);
}
static int tasklistInstalled(void)
{
    HKEY key;
    char value[MAX_PATH+16], command[MAX_PATH+16];
    DWORD size=sizeof(value)-1, type=0;
    LONG status;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,WINLOGON_KEY,0,KEY_QUERY_VALUE,&key)) return 0;
    status=RegQueryValueExA(key,"Taskman",NULL,&type,(LPBYTE)value,&size);
    RegCloseKey(key);
    if (status || type!=REG_SZ) return 0;
    value[size]=0;
    tasklistCommand(command);
    return !lstrcmpiA(value,command);
}
static void updateTasklistMenu(void)
{
    if (!tasklistSupported()) return;
    CheckMenuItem(GetMenu(mainWindow),ID_TASKLIST,MF_BYCOMMAND|(tasklistInstalled()?MF_CHECKED:MF_UNCHECKED));
}
static void toggleTasklist(HWND hwnd)
{
    HKEY key;
    char command[MAX_PATH+16], text[160];
    int installed=tasklistInstalled();
    LONG status;
    tasklistCommand(command);
    status=RegOpenKeyExA(HKEY_LOCAL_MACHINE,WINLOGON_KEY,0,KEY_SET_VALUE,&key);
    if (!status) {
        status=installed ? RegDeleteValueA(key,"Taskman")
            : RegSetValueExA(key,"Taskman",0,REG_SZ,(LPBYTE)command,(DWORD)lstrlenA(command)+1);
        RegCloseKey(key);
    }
    if (status) wsprintfA(text,"Could not change the Task List setting (error %ld). Administrator rights are needed.",status);
    else lstrcpyA(text,installed ? "Program Manager's Task List returns at next logon."
        : "From the next logon, Ctrl+Esc opens NTPERF3X in place of the Task List.");
    MessageBoxA(hwnd,text,"Replace Task List",MB_OK|(status?MB_ICONSTOP:MB_ICONINFORMATION));
}
static int tasklistArgument(void)
{
    char *line=GetCommandLineA();
    int n=lstrlenA(line);
    return n>=9 && !lstrcmpiA(line+n-9,"/tasklist");
}
/* Started by Program Manager, which then skips its own Task List: stay hidden
 * until Ctrl+Esc or SC_TASKLIST, and hide instead of closing, as nothing else
 * will bring a Task List back. Without Ctrl+Esc, a desktop double-click or
 * running NTPERF3X again still opens it. High priority, like the original,
 * keeps it responsive while an app spins. */
static void startTasklist(void)
{
    union { FARPROC generic; REGISTER_TASKLIST registerTasklist; } fn;
    if (!tasklistSupported() || !tasklistArgument()) return;
    fn.generic=GetProcAddress(GetModuleHandleA("user32.dll"),"RegisterTasklist");
    if (fn.registerTasklist) fn.registerTasklist(mainWindow);
    if (!RegisterHotKey(mainWindow,1,MOD_CONTROL,VK_ESCAPE))
        MessageBoxA(NULL,"Another program is using Ctrl+Esc. Double-click the desktop to open the Task List.",
            "NTPERF3X",MB_OK|MB_ICONEXCLAMATION);
    SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
    tasklistMode=1;
}
/* Sampling stops while hidden; restart the deltas as Pause does. */
static void showTasklist(int apps)
{
    if (!IsWindowVisible(mainWindow) && !paused) {
        previousValid=0;
        processBaseline=0;
        takeSample();
    }
    ShowWindow(mainWindow,IsIconic(mainWindow)?SW_RESTORE:SW_SHOW);
    SetForegroundWindow(mainWindow);
    if (apps) setTab(TAB_APPS);
}
