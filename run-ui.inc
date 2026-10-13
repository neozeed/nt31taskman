/* Empty, DWORD-aligned dialog template; controls use only USER32. */
static NP_DLGRESULT CALLBACK runDialogProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    HWND control;
    RECT owner, bounds;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    static char command[1024], expanded[4096];
    char error[256];
    DWORD length, code;
    int id;
    (void)lp;
    if (message == WM_INITDIALOG) {
        SetWindowTextA(hwnd,"Run");
        GetWindowRect(GetParent(hwnd),&owner);
        GetWindowRect(hwnd,&bounds);
        SetWindowPos(hwnd,NULL,owner.left+(owner.right-owner.left-(bounds.right-bounds.left))/2,
            owner.top+(owner.bottom-owner.top-(bounds.bottom-bounds.top))/2,
            0,0,SWP_NOSIZE|SWP_NOZORDER);
        control=CreateWindowA("STATIC","&Command line:",WS_CHILD|WS_VISIBLE,
            12,12,360,18,hwnd,(HMENU)200,instance,NULL);
        SendMessageA(control,WM_SETFONT,(WPARAM)uiFont,FALSE);
        control=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,
            12,34,360,24,hwnd,(HMENU)201,instance,NULL);
        SendMessageA(control,WM_SETFONT,(WPARAM)uiFont,FALSE);
        SendMessageA(control,EM_LIMITTEXT,1023,0);
        SetFocus(control);
        control=CreateWindowA("BUTTON","OK",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
            204,72,80,25,hwnd,(HMENU)IDOK,instance,NULL);
        SendMessageA(control,WM_SETFONT,(WPARAM)uiFont,FALSE);
        control=CreateWindowA("BUTTON","Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP,
            292,72,80,25,hwnd,(HMENU)IDCANCEL,instance,NULL);
        SendMessageA(control,WM_SETFONT,(WPARAM)uiFont,FALSE);
        /* Pixel-sized controls: fit the client independently of dialog units. */
        SetRect(&bounds,0,0,384,110);
        AdjustWindowRect(&bounds,WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME,FALSE);
        SetWindowPos(hwnd,NULL,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,
            SWP_NOMOVE|SWP_NOZORDER);
        return FALSE;
    }
    if (message == WM_CLOSE) { EndDialog(hwnd,IDCANCEL); return TRUE; }
    if (message != WM_COMMAND) return FALSE;
    id=LOWORD(wp);
    if (id == IDCANCEL) { EndDialog(hwnd,IDCANCEL); return TRUE; }
    if (id != IDOK) return FALSE;
    GetDlgItemTextA(hwnd,201,command,sizeof(command));
    length=0;
    while (command[length]==' ' || command[length]=='\t') ++length;
    if (!command[length]) { SetFocus(GetDlgItem(hwnd,201)); return TRUE; }
    length=ExpandEnvironmentStringsA(command,expanded,sizeof(expanded));
    if (!length || length > sizeof(expanded)) {
        MessageBoxA(hwnd,"The expanded command line is too long or could not be expanded.","Run",MB_OK|MB_ICONEXCLAMATION);
        return TRUE;
    }
    clearBytes(&startup,sizeof(startup)); startup.cb=sizeof(startup);
    clearBytes(&process,sizeof(process));
    if (!CreateProcessA(NULL,expanded,NULL,NULL,FALSE,0,NULL,NULL,&startup,&process)) {
        code=GetLastError();
        wsprintfA(error,"Could not start the program (Windows error %lu).\nCheck the command and quote executable paths containing spaces.",code);
        MessageBoxA(hwnd,error,"Run",MB_OK|MB_ICONEXCLAMATION);
        SetFocus(GetDlgItem(hwnd,201));
        return TRUE;
    }
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    EndDialog(hwnd,IDOK);
    return TRUE;
}

static void showRunDialog(HWND owner)
{
    union { DWORD alignment; BYTE bytes[64]; } templateBuffer;
    DLGTEMPLATE *dialog;
    clearBytes(&templateBuffer,sizeof(templateBuffer));
    dialog=(DLGTEMPLATE *)templateBuffer.bytes;
    dialog->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;
    dialog->cx=240; dialog->cy=75;
    /* Zero cdit and trailing zero WORDs specify no menu/class/title. */
    if (DialogBoxIndirectParamA(instance,dialog,owner,runDialogProc,0)==-1)
        MessageBoxA(owner,"Could not open the Run dialog.","Run",MB_OK|MB_ICONEXCLAMATION);
}
