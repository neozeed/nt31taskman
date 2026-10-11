@echo off
rem Run from an x86 NT 3.1 SDK / Visual C++ build environment.
cl /nologo /W3 /O1 /DNTPERF_USE_CRT /c ntperf31.c
if errorlevel 1 goto end
link /nologo /subsystem:windows,3.10 /out:NTPERF31.EXE ntperf31.obj kernel32.lib user32.lib gdi32.lib
:end
