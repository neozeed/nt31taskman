@echo off
rem Run from an x86 NT 3.1 SDK / Visual C++ build environment.
cl /nologo /W3 /O1 /DNTPERF_USE_CRT /c ntperf3x.c nt3x.c
if errorlevel 1 goto end
link /nologo /subsystem:windows,3.10 /out:NTPERF3X.EXE ntperf3x.obj nt3x.obj kernel32.lib user32.lib gdi32.lib
:end
