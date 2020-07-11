// File: KernelSystem.cpp
// Summary: KernelSystem class implementation file.

#include <algorithm>
#include "KernelSystem.h"
#include "KernelProcess.h"
#include "part.h"

KernelSystem::KernelSystem(PhysicalAddress processVMSpace,
    PageNum processVMSpaceSize, PhysicalAddress pmtSpace,
    PageNum pmtSpaceSize, Partition *partition):
    swapPartition_(partition),diskSpaceManager_((partition) ? partition->getNumOfClusters() : 0),
    processSpaceManager_(processVMSpace, processVMSpaceSize), pmtSpaceManager_(pmtSpace, pmtSpaceSize),
    processSpace_(processVMSpace), usedPids_(), nextUnusedPid_(0), pmtp_()
{

}

KernelSystem::~KernelSystem()
{

}

KernelProcess *KernelSystem::createProcess()
{
    std::lock_guard<std::mutex> lock(mutex_guard_);

    // determine the pid
    ProcessId pid;
    if (!usedPids_.empty())
    {
        pid = usedPids_.top();
        usedPids_.pop();
    }
    else
    {
        if (nextUnusedPid_ == (ProcessId)-1)
        {
            return nullptr;
        }
        pid = nextUnusedPid_++;
    }

    // allocate and initialize level 0 pmt for this process
    PmtEntry0 *pmt0 = (PmtEntry0 *)pmtSpaceManager_.alloc();
    if (pmt0 == nullptr)
    {
        usedPids_.push(pid);
        return nullptr;
    }

    for (int i = 0; i < PMT_0_NUM_ENTRIES; ++i)
    {
        pmt0[i].pmt1 = nullptr;
    }

    // allocate the process
    KernelProcess *proc;
    proc = new KernelProcess(pid, pmt0, this);
    if (proc == nullptr)
    {
        usedPids_.push(pid);
        pmtSpaceManager_.dealloc(pmt0);
        return nullptr;
    }

    return proc;
}

KernelProcess *KernelSystem::cloneProcess(ProcessId targetPid)
{
    std::lock_guard<std::mutex> lock(mutex_guard_);

    // find targeted process
    auto it = pmtp_.find(targetPid);
    if (it == pmtp_.end())
    {
        return nullptr; // no process with pid found
    }

    // determine the pid
    ProcessId pid;
    if (!usedPids_.empty())
    {
        pid = usedPids_.top();
        usedPids_.pop();
    }
    else
    {
        if (nextUnusedPid_ == (ProcessId)-1)
        {
            return nullptr;
        }
        pid = nextUnusedPid_++;
    }

    KernelProcess *newProcess = it->second->clone(pid);
    if (!newProcess)
    {
        usedPids_.push(pid);
        return nullptr;
    }

    return newProcess;
}

Status KernelSystem::access(ProcessId pid, VirtualAddress address, AccessType type)
{
    if (!IS_VADDR_VALID(address))
    {
        return TRAP;
    }

    auto it = pmtp_.find(pid);
    if (it == pmtp_.end())
    {
        return TRAP; // no process with pid found
    }

    PmtEntry0 *pmt0 = it->second->pmt0_;
    int pmt0Entry = VADDR_PMT0_ENTRY(address);
    int pmt1Entry = VADDR_PMT1_ENTRY(address);
    int offset = VADDR_OFFSET(address);

    if (pmt0[pmt0Entry].pmt1 == nullptr)
    {
        return TRAP; // memory access violation
    }
    if (!BIT_IS_SET(pmt0[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_VALID))
    {
        return TRAP; // memory access violation
    }

    switch (type)
    {
    case READ:
        if (!BIT_IS_SET(pmt0[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_READ))
        {
            return TRAP; // memory access violation
        }
        break;
    case WRITE:
        if (!BIT_IS_SET(pmt0[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_WRITE))
        {
            return TRAP; // memory access violation
        }
        break;
    case READ_WRITE:
        if (!BIT_IS_SET(pmt0[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_READ))
        {
            return TRAP; // memory access violation
        }
        if (!BIT_IS_SET(pmt0[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_WRITE))
        {
            return TRAP; // memory access violation
        }
        break;
    case EXECUTE:
        if (!BIT_IS_SET(pmt0[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_EXEC))
        {
            return TRAP; // memory access violation
        }
        break;
    default:
        return TRAP; // invalid access type
        break;
    }

    if (!BIT_IS_SET(pmt0[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_MAPPED))
    {
        return PAGE_FAULT;
    }

    if (type == WRITE || type == READ_WRITE)
    {
        BIT_SET(pmt0[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_DIRTY);
    }

    return OK;
}

Time KernelSystem::periodicJob()
{
    return 0;
}

ProcessId KernelSystem::getAvailablePid()
{
    std::lock_guard<std::mutex> lock(mutex_guard_);

    ProcessId pid;
    if (!usedPids_.empty())
    {
        pid = usedPids_.top();
        usedPids_.pop();
        return pid;
    }
    else
    {
        if (nextUnusedPid_ == (ProcessId)-1)
        {
            return nextUnusedPid_;
        }
        pid = nextUnusedPid_++;
        return pid;
    }
}

void KernelSystem::registerProcess(KernelProcess *proc)
{
    pmtp_[proc->pid_] = proc;
}

void KernelSystem::unregisterProcess(KernelProcess *proc)
{
    usedPids_.push(proc->pid_);
    pmtp_.erase(proc->pid_);
}

