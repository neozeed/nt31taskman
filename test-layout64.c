/* Compile-only independent check against the SDK's x64 native structure. */
#include <windows.h>
#include <winternl.h>
#include <stddef.h>
typedef char check_pointer[(sizeof(void *)==8)?1:-1];
typedef char check_header[(sizeof(SYSTEM_PROCESS_INFORMATION)==0x100)?1:-1];
typedef char check_name[(offsetof(SYSTEM_PROCESS_INFORMATION,ImageName)==0x38)?1:-1];
typedef char check_pid[(offsetof(SYSTEM_PROCESS_INFORMATION,UniqueProcessId)==0x50)?1:-1];
typedef char check_vm[((offsetof(SYSTEM_PROCESS_INFORMATION,VirtualMemoryCounters)+offsetof(VM_COUNTERS,VirtualSize))==0x78)?1:-1];
typedef char check_ws[((offsetof(SYSTEM_PROCESS_INFORMATION,VirtualMemoryCounters)+offsetof(VM_COUNTERS,WorkingSetSize))==0x90)?1:-1];
typedef char check_created[(offsetof(SYSTEM_PROCESS_INFORMATION,CreateTime)==0x20)?1:-1];
typedef char check_user[(offsetof(SYSTEM_PROCESS_INFORMATION,UserTime)==0x28)?1:-1];
typedef char check_kernel[(offsetof(SYSTEM_PROCESS_INFORMATION,KernelTime)==0x30)?1:-1];
typedef char check_priority[(offsetof(SYSTEM_PROCESS_INFORMATION,BasePriority)==0x48)?1:-1];
typedef char check_handles[(offsetof(SYSTEM_PROCESS_INFORMATION,HandleCount)==0x60)?1:-1];
typedef char check_faults[((offsetof(SYSTEM_PROCESS_INFORMATION,VirtualMemoryCounters)+offsetof(VM_COUNTERS,PageFaultCount))==0x80)?1:-1];
typedef char check_peak[((offsetof(SYSTEM_PROCESS_INFORMATION,VirtualMemoryCounters)+offsetof(VM_COUNTERS,PeakWorkingSetSize))==0x88)?1:-1];
typedef char check_paged[((offsetof(SYSTEM_PROCESS_INFORMATION,VirtualMemoryCounters)+offsetof(VM_COUNTERS,QuotaPagedPoolUsage))==0xa0)?1:-1];
typedef char check_nonpaged[((offsetof(SYSTEM_PROCESS_INFORMATION,VirtualMemoryCounters)+offsetof(VM_COUNTERS,QuotaNonPagedPoolUsage))==0xb0)?1:-1];
typedef char check_commit[(offsetof(SYSTEM_PROCESS_INFORMATION,PrivatePageCount)==0xc8)?1:-1];
