// File: Process.cpp
// Summary: Process class implementation file.

#include "Process.h"
#include "KernelProcess.h"

Process::Process(ProcessId pid):
    pProcess(nullptr)
{

}

Process::~Process()
{

}

ProcessId Process::getProcessId() const
{
    return pProcess->getProcessId();
}

Status Process::createSegment(VirtualAddress startAddress, PageNum segmentSize,
                              AccessType flags)
{
    return pProcess->createSegment(startAddress, segmentSize, flags);
}

Status Process::loadSegment(VirtualAddress startAddress, PageNum segmentSize,
                            AccessType flags, void *content)
{
    return pProcess->loadSegment(startAddress, segmentSize, flags, content);
}

Status Process::deleteSegment(VirtualAddress startAddress)
{
    return pProcess->deleteSegment(startAddress);
}

Status Process::pageFault(VirtualAddress startAddress)
{
    return pProcess->pageFault(startAddress);
}

PhysicalAddress Process::getPhysicalAddress(VirtualAddress address)
{
    return pProcess->getPhysicalAddress(address);
}

Process *Process::clone(ProcessId pid)
{
    (void)pid; // ignore this parameter
    Process *newProcess = new Process(0); // allocate empty object
    if (newProcess == nullptr)
    {
        return nullptr;
    }
    newProcess->pProcess = pProcess->clone();
    if (newProcess->pProcess == nullptr)
    {
        delete newProcess;
        return nullptr;
    }
    return newProcess;
}

Status Process::createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char *name, AccessType flags)
{
    return pProcess->createSharedSegment(startAddress, segmentSize, name, flags);
}

Status Process::disconnectSharedSegment(const char *name)
{
    return pProcess->disconnectSharedSegment(name);
}

Status Process::deleteSharedSegment(const char *name)
{
    return pProcess->deleteSharedSegment(name);
}

// for testing purposes
std::ostream & operator<<(std::ostream &os, const Process &p)
{
    os << *p.pProcess;
    return os;
}

