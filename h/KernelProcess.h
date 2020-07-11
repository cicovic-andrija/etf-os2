// File: KernelProcess.h
// Summary: KernelProcess class header file.

#ifndef VM_EMU_KERNEL_PROCESS_H
#define VM_EMU_KERNEL_PROCESS_H

#include <vector>
#include <mutex>
#include <stack>
#include <string>
#include <unordered_map>
#include "vm_declarations.h"
#include "descr.h"
#include "SegmentDescr.h"
#include "SharedSegmentDescr.h"

// for testing purposes
#include <iostream>
#include <iomanip>

class KernelSystem;

class KernelProcess {
public:
    KernelProcess(ProcessId pid, PmtEntry0 *pmt0, KernelSystem *system);
    ~KernelProcess();

    ProcessId getProcessId() const;
    Status createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags);
    Status loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void *content);
    Status deleteSegment(VirtualAddress startAddress);
    PhysicalAddress getPhysicalAddress(VirtualAddress address);
    Status pageFault(VirtualAddress startAddress);
    KernelProcess *clone(ProcessId pid);
    KernelProcess *clone();
    Status createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char *name, AccessType flags);
    Status disconnectSharedSegment(const char *name);
    Status deleteSharedSegment(const char *name);

    // for testing purposes
    friend std::ostream &operator<<(std::ostream &os, const KernelProcess &kp);
private:
    friend class KernelSystem;

    // page replacement
    static PmtEntry1 *clockHand;
    static PmtEntry1 *getVictim();

    // shared segment support
    static std::stack<unsigned int> usedSharedSegmentIds;
    static unsigned int nextUnusedSharedSegmentId;
    static std::unordered_map<std::string, SharedSegmentDescr *> sharedSegments;
    static SharedSegmentDescr *findSharedSegmentById(unsigned int id);

    const ProcessId pid_;
    KernelSystem *system_;
    PmtEntry0 *pmt0_;
    std::vector<SegmentDescr> segments_;
    std::mutex mutex_guard_;

    Status validateSegmentInfo(VirtualAddress startAddr, PageNum segmentSize, AccessType flags);
    Status addSegment(VirtualAddress startAddr, PageNum segmentSize, AccessType flags);
    Status addSharedSegment(VirtualAddress startAddr, PageNum segmentSize, const char *name, AccessType flags);
    Status connectToSharedSegment(SharedSegmentDescr *descr);
    Status removeSegment(VirtualAddress startAddr);
    Status initPmt1Entries(const SegmentDescr &segmDescr, unsigned int sharedSegmentId);
    Status releasePmt1Entries(const SegmentDescr &segmDescr, bool releaseResources);
    bool invalidateEntries(int pmt0Entry, int pmt1StartEntry, int pmt1EndEntry, bool releaseResources);
};

#endif // VM_EMU_KERNEL_PROCESS_H

