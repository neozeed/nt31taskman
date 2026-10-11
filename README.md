# NT Performance — NTPERF31

### Windows NT 3.1, now with substantially more green graphs.

Windows NT 3.1 has the blue title bars. It has Program Manager. It has File Manager. It has a proper operating system lurking beneath a desktop that looks as though it would like you to fax it something.

What it could really use is that familiar NT 4-style window: **processes, CPU usage, memory statistics, and little green lines that tell you the machine is doing something.**

So here it is.

**NTPERF31 is a small, native x86 performance monitor and process manager for released Windows NT 3.1.** It brings an NT 4-inspired Processes and Performance interface to the older system, while keeping NT 3.1's own window frame, menus, and wonderfully unapologetic grey desktop furniture.

It has been run on **Windows NT 3.1 SP3 under QEMU**, with both the performance graphs and process list working.

**C89. Plain Win32. Approximately 50 KB of executable. No installer.**

<!-- Optional screenshots: add cropped, application-only images at these paths,
then remove this comment wrapper to display them.

![Performance graphs running on Windows NT 3.1](docs/images/performance.png)
![Processes view running on Windows NT 3.1](docs/images/processes.png)

-->

## The good stuff

### Performance

The important bit: the green graphs.

- Scrolling **CPU usage history** on a black grid.
- A segmented CPU meter, because some design decisions were already correct in the 1990s.
- **Committed-memory history** and a matching usage meter.
- Optional **red kernel CPU overlay**, excluding idle time.
- Physical memory, commit charge, and paged/nonpaged kernel pool figures.
- System-wide process, thread, and handle totals.
- A **180-sample history**, with 0.5, 1, or 2-second updates.
- Pause, manual refresh, and always-on-top.

### Processes

Find out what is actually occupying your extremely valuable megabytes.

| Column | What it shows |
|---|---|
| Image Name | Process image name, or a PID-based label when no name is supplied |
| PID | Process identifier |
| CPU | Sampled CPU percentage, normalized across the machine's processors |
| CPU Time | Accumulated user + kernel execution time |
| Mem Usage | Working set, in KiB |
| VM Size | Virtual address-space size, in KiB |
| Threads | Number of threads in the process |

Click a column heading to sort. Click it again to reverse the order. The selected process stays selected across refreshes and sorting, even when other processes move around it.

There is also an **End Process** button. Sometimes the most useful performance optimization is making the offending program stop existing.

## Run it

Copy `NTPERF31.EXE` onto your NT 3.1 machine and launch it from File Manager, Program Manager, or a command prompt:

```bat
NTPERF31.EXE
```

The **Processes** page opens first. Click **Performance** for the graphs.

Allow a couple of samples for CPU percentages to appear. A new process initially shows `--` because calculating usage requires two observations. The Idle process reports idle CPU time, so seeing it near 100% is good news.

The supplied GNU-built executable requires no additional runtime DLLs.

## Controls

| Action | Control |
|---|---|
| Switch pages | Click Processes / Performance, or press **Ctrl+Tab** |
| Move between controls | **Tab** |
| Navigate processes | Arrow keys and Page Up / Page Down |
| Sort processes | Click a column heading |
| Refresh immediately | **F5** |
| Pause automatic updates | **View → Paused** |
| Change update speed | **View** menu |
| Show busy kernel CPU time | **View → Show kernel times** |
| Keep the window above other windows | **Options → Always on top** |
| Terminate the selected process | **End Process**, or **Delete** with the list focused |
| Copy diagnostic information | **Help → Copy diagnostics** |

Both pages continue collecting samples while you switch between them. Changing update speed clears graph history. The history axis represents samples; manually refreshing or pausing changes their spacing in real time.

## About that End Process button

**End Process forcibly terminates the selected process. Unsaved work in it will be lost.** It is not a polite request to close an application window.

Before termination, NTPERF31:

1. Opens a handle to the selected process.
2. Checks its creation time against the selected entry.
3. Asks for confirmation, showing the name and PID, with **No** selected by default.
4. Uses that same open handle if you confirm.

That matters because PIDs can be reused. A process disappearing while you look at the list must not turn into permission to kill whatever happens to inherit its number.

The button is disabled for the monitor itself, Idle, the NT 3.1 System process, and several recognized critical system processes. Access failures are reported rather than worked around by enabling debug privilege.

For a first test, use a disposable Notepad instance. NT may be robust, but your unsaved document is still just an unsaved document.

## What the memory numbers mean

A few labels deserve explanation before someone starts adding every column together with a calculator.

- **Memory Usage History** tracks **commit charge**, scaled to the current commit limit. It is not a graph of physical RAM usage.
- **Physical Memory → Used** is total physical memory minus available physical memory.
- **Mem Usage** in the process table is the process working set. Shared pages can appear in multiple working sets, so summing the column does not give physical memory usage.
- **VM Size** is reserved plus committed virtual address space. It is not private committed memory.
- **Kernel Pool Memory → Total** is paged pool plus nonpaged pool, not everything the kernel occupies.
- **K** means KiB.

The physical-memory panel uses **Used** in place of the reference Task Manager's File Cache row.

## Under the bonnet

NTPERF31 uses ordinary **USER32 controls and GDI drawing**. The tabs are owner-drawn buttons; the process table is an owner-drawn listbox. There is no dependency on later common controls.

The GNU build directly imports only:

- `KERNEL32.DLL`
- `USER32.DLL`
- `GDI32.DLL`

It resolves `NtQuerySystemInformation` dynamically from `NTDLL.DLL` to collect native statistics.

**No COMCTL32, PSAPI, Toolhelp, PDH, MSVCRT, or kernel driver is required by the supplied build.** The PE operating-system and GUI subsystem versions are set to **3.10**, and the compiler targets the **i386 instruction set**.

The old structure layouts matter. For example, the field at offset `0x4c` in an NT 3.1 process record is an I/O counter. Later versions use that location for a handle count. Reading it with the wrong definition can produce impressively detailed nonsense.

The decoder therefore uses the released NT 3.1 x86 layout explicitly, validates record links and Unicode-name bounds, and identifies processes by **PID plus creation time**. If a process snapshot fails, the previous rows remain visible with a stale-data message and End Process is disabled.

A small executable can still have manners.

## Build from source

With **GNU make** and an **i686 MinGW-w64 C compiler** installed, run this from the source directory:

```sh
make
```

Or specify the compiler explicitly:

```sh
make CC=i686-w64-mingw32-gcc
```

Output:

```text
NTPERF31.EXE
```

The GNU build uses its own entry point and links without the C runtime. The source is C89 and builds with strict warnings enabled.

An alternative `build-ms.cmd` is included for an **x86 NT 3.1 SDK / contemporary Microsoft compiler environment**. That route uses the compiler's normal runtime startup and remains untested. A modern Microsoft runtime should not be assumed to support NT 3.1.

### Optional tests

The counter arithmetic and process-record decoder can be tested on the build host:

```sh
gcc -std=c89 -pedantic -Wall -Wextra -Werror test-counters.c -o test-counters
./test-counters

gcc -std=c89 -pedantic -Wall -Wextra -Werror test-process.c -o test-process
./test-process
```

These cover counter rollover and reset, multi-CPU normalization, historical process offsets, malformed records, string bounds, and process identity when a PID is reused.

## Status and scope

**Current version: R2.**

- **R1:** Performance page, CPU/memory histories, meters, and system totals.
- **R2:** Processes page, sortable columns, stable selection, and End Process.

Both views have been demonstrated running on **NT 3.1 SP3 under QEMU**. Local compilation, counter/decoder tests, and PE/header/import checks also pass. The screenshots establish the display and enumeration results; they do not constitute exhaustive testing of termination, every menu action, or multiprocessor hardware.

This is an NT 4-inspired utility written for NT 3.1, not a port of Microsoft's Task Manager source. There is no Applications page yet. The supplied executable is **x86 only**; MIPS, Alpha, early NT betas, and other Windows releases are not claimed as tested targets.

## Reporting problems

Please include:

- NT version and service pack.
- Emulator or hardware, processor count, and RAM.
- What happened, and what you expected.
- The output of **Help → Copy diagnostics**.
- An application-window screenshot if the problem is visual.

If the executable cannot start, include the exact loader error instead.

## References

Historical native structure layouts were checked against **Geoff Chappell's original research**:

- [System process information](https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/process.htm)
- [System performance information](https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/performance.htm)
- [Processor performance information](https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/processor_performance.htm)
- [System handle table entries](https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/handle_table_entry.htm)

The visual inspiration is the classic Windows NT 4 Task Manager: grey panels, black graphs, green traces, useful numbers. A perfectly respectable amount of computer.

---

*Your 1993 desktop has received its green-graph upgrade. Please resume staring at CPU usage instead of doing whatever you opened the computer to do.*
