// File: System.h
// Summary: System class header file.

#ifndef VM_EMU_SYSTEM_H
#define VM_EMU_SYSTEM_H

#include "vm_declarations.h"

class Partition;
class Process;
class KernelProcess;
class KernelSystem;

class System {
public:
    System(PhysicalAddress processVMSpace, PageNum processVMSpaceSize,
           PhysicalAddress pmtSpace, PageNum pmtSpaceSize,
           Partition *partition);
    ~System();

    Process *createProcess();
    Time periodicJob();
    Process *cloneProcess(ProcessId pid);

    // Hardware job
    Status access(ProcessId pid, VirtualAddress address, AccessType type);

private:
    friend class Process;
    friend class KernelProcess;

    KernelSystem *pSystem;
};

#endif // VM_EMU_SYSTEM_H

