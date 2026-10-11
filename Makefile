CC = i686-w64-mingw32-gcc
CFLAGS = -Os -std=c89 -pedantic -Wall -Wextra -Werror -march=i386 -mno-sse -mno-sse2 -fno-builtin -fno-stack-protector
LDFLAGS = -nostdlib -Wl,--entry,_WinMainCRTStartup -Wl,--subsystem,windows:3.10 -Wl,--major-os-version,3,--minor-os-version,10 -Wl,--disable-dynamicbase,--disable-nxcompat -Wl,--no-insert-timestamp

all: NTPERF31.EXE

NTPERF31.EXE: ntperf31.c counters.h process.h process-ui.inc
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -luser32 -lgdi32 -lkernel32

clean:
	rm -f NTPERF31.EXE

.PHONY: all clean
