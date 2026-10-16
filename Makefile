# Nmake macros for building Windows 32-Bit apps

!include <ntwin32.mak>

all: taskman.exe

OBJS= NTPERF31.obj nt31.obj

NTPERF31.obj: NTPERF31.C backend.h process.h numbers.h counters.h process-ui.inc run-ui.inc
    $(cc) $(cflags) $(cvars) NTPERF31.C

nt31.obj: nt31.c backend.h process.h process-nt31.h native-query.inc
    $(cc) $(cflags) $(cvars) nt31.c

TEST_COU.obj: TEST_COU.C
    $(cc) $(cflags) $(cvars) TEST_COU.C

TEST_PRO.obj: TEST_PRO.C
    $(cc) $(cflags) $(cvars) TEST_PRO.c

taskman.exe: $(OBJS)
    $(link) $(guiflags) -out:taskman.exe $(OBJS) $(guilibs)

testc.exe: TEST_COU.obj
    $(link) $(conflags) -out:testc.exe TEST_COU.obj $(conlibs)

testp.exe: TEST_PRO.obj
    $(link) $(conflags) -out:testc.exe TEST_PRO.obj $(conlibs)

clean:
    -del *.obj
    -del taskman.exe
    -del testc.exe
