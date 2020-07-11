// File: KernelSystem.h
// Summary: KernelSystem class header file.

#ifndef VM_EMU_KERNEL_SYSTEM_H
#define VM_EMU_KERNEL_SYSTEM_H

#include <stack>
#include <mutex>
#include <unordered_map>
#include "vm_declarations.h"
#include "FrameAllocator.h"
#include "ClusterManager.h"

class Partition;
class KernelProcess;
struct PmtEntry0;

class KernelSystem {
public:
    KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize,
        PhysicalAddress pmtSpace, PageNum pmtSpaceSize,
        Partition *partition);
    ~KernelSystem();

    KernelProcess *createProcess();
    KernelProcess *cloneProcess(ProcessId pid);
    Time periodicJob();
    Status access(ProcessId pid, VirtualAddress address, AccessType type); // hardware job

private:
    friend class KernelProcess;

    FrameAllocator processSpaceManager_;
    PhysicalAddress processSpace_;
    FrameAllocator pmtSpaceManager_;
    ClusterManager diskSpaceManager_;
    Partition *swapPartition_;
    ProcessId nextUnusedPid_;
    std::stack<ProcessId> usedPids_;
    std::unordered_map<ProcessId, KernelProcess *> pmtp_;
    std::mutex mutex_guard_;

    ProcessId getAvailablePid();
    void registerProcess(KernelProcess *proc);
    void unregisterProcess(KernelProcess *proc);
};

#endif // VM_EMU_KERNEL_SYSTEM_H

