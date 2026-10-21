# Nmake macros for building Windows 32-Bit apps

!include <ntwin32.mak>

all: taskman.exe

OBJS= NTPERF3X.obj nt3x.obj

NTPERF3X.obj: NTPERF3X.C backend.h process.h numbers.h counters.h process-ui.inc apps-ui.inc run-ui.inc tasklist-ui.inc
    $(cc) $(cflags) $(cvars) NTPERF3X.C

nt3x.obj: nt3x.c backend.h process.h process-nt3x.h native-query.inc
    $(cc) $(cflags) $(cvars) nt3x.c

TEST_COU.obj: TEST_COU.C
    $(cc) $(cflags) $(cvars) TEST_COU.C

TEST_PRO.obj: TEST_PRO.C
    $(cc) $(cflags) $(cvars) TEST_PRO.c

taskman.exe: $(OBJS)
    $(link) $(guiflags) -out:taskman.exe $(OBJS) $(guilibs) advapi32.lib

testc.exe: TEST_COU.obj
    $(link) $(conflags) -out:testc.exe TEST_COU.obj $(conlibs)

testp.exe: TEST_PRO.obj
    $(link) $(conflags) -out:testc.exe TEST_PRO.obj $(conlibs)

clean:
    -del *.obj
    -del taskman.exe
    -del testc.exe
