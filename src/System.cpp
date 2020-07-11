// File: System.cpp
// Summary: System class implementation file.

#include "System.h"
#include "KernelSystem.h"
#include "Process.h"

System::System(PhysicalAddress processVMSpace, PageNum processVMSpaceSize,
               PhysicalAddress pmtSpace, PageNum pmtSpaceSize,
               Partition * partition)
{
    pSystem = new KernelSystem(processVMSpace, processVMSpaceSize,
                               pmtSpace, pmtSpaceSize, partition);
}

System::~System()
{

}

Process *System::createProcess()
{
    Process *newProcess = new Process(0); // allocate empty object
    if (newProcess == nullptr) return nullptr;
    newProcess->pProcess = pSystem->createProcess(); // assign kernel process object
    if (newProcess->pProcess == nullptr)
    {
        delete newProcess;
        return nullptr;
    }
    return newProcess;
}

Time System::periodicJob()
{
    return pSystem->periodicJob();
}

Process *System::cloneProcess(ProcessId pid)
{
    Process *newProcess = new Process(0); // allocate empty object
    if (newProcess == nullptr)
    {
        return nullptr;
    }
    newProcess->pProcess = pSystem->cloneProcess(pid);
    if (newProcess->pProcess == nullptr)
    {
        delete newProcess;
        return nullptr;
    }
    return newProcess;
}

Status System::access(ProcessId pid, VirtualAddress address, AccessType type)
{
    return pSystem->access(pid, address, type);
}

