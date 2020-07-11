// File: Process.h
// Summary: Process class header file.

#ifndef VM_EMU_PROCESS_H
#define VM_EMU_PROCESS_H

#include "vm_declarations.h"

// for testing purposes
#include <iostream>

class System;
class KernelSystem;
class KernelProcess;

class Process {
public:
    Process(ProcessId pid);
    ~Process();

    ProcessId getProcessId() const;
    Status createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags);
    Status loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void *content);
    Status deleteSegment(VirtualAddress startAddress);
    Status pageFault(VirtualAddress startAddress);
    PhysicalAddress getPhysicalAddress(VirtualAddress address);

    // support for shared segments
    Process *clone(ProcessId pid);
    Status createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char *name, AccessType flags);
    Status disconnectSharedSegment(const char *name);
    Status deleteSharedSegment(const char *name);

    // for testing purposes
    friend std::ostream &operator<<(std::ostream &os, const Process &p);
private:
    friend class System;
    friend class KernelSystem;

    KernelProcess *pProcess;
};

#endif // VM_EMU_PROCESS_H

