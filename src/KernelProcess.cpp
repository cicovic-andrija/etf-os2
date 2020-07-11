// File: KernelProcess.cpp
// Summary: KernelProcess class implementation file.

#include <queue>
#include <cstring>
#include <algorithm>
#include "KernelProcess.h"
#include "KernelSystem.h"

// init. clockHand for the "clock algorithm" for page replacement
PmtEntry1 *KernelProcess::clockHand = nullptr;

std::stack<unsigned int> KernelProcess::usedSharedSegmentIds;

// 0 is not a valid shared segment id
unsigned int KernelProcess::nextUnusedSharedSegmentId = 1;

std::unordered_map<std::string, SharedSegmentDescr *> KernelProcess::sharedSegments;

KernelProcess::KernelProcess(ProcessId pid, PmtEntry0 *pmt0, KernelSystem *system):
    pid_(pid), pmt0_(pmt0), system_(system), segments_()
{
    if (system_)
    {
        system_->registerProcess(this);
    }
}

KernelProcess::~KernelProcess()
{
    for (auto it = segments_.begin(); it != segments_.end(); ++it)
    {
        deleteSegment((*it).startAddr_);
    }
    if (system_)
    {
        system_->unregisterProcess(this);
    }
}

ProcessId KernelProcess::getProcessId() const
{
    return pid_;
}

Status KernelProcess::createSegment(VirtualAddress startAddress,
                                    PageNum segmentSize, AccessType flags)
{
    if (validateSegmentInfo(startAddress, segmentSize, flags) != OK)
    {
        return TRAP;
    }

    if (addSegment(startAddress, segmentSize, flags) != OK)
    {
        return TRAP;
    }

    return OK;
}

Status KernelProcess::loadSegment(VirtualAddress startAddress,
                                  PageNum segmentSize,
                                  AccessType flags, void *content)
{
    if (validateSegmentInfo(startAddress, segmentSize, flags) != OK)
    {
        return TRAP;
    }

    if (addSegment(startAddress, segmentSize, flags) != OK)
    {
        return TRAP;
    }

    std::vector<ClusterNo> clustersTaken;
    for (unsigned i = 0; i < segmentSize; ++i)
    {
        ClusterNo cluster;
        bool success = system_->diskSpaceManager_.takeCluster(cluster);
        if (!success) // not enough clusters on disk
        {
            for (auto it = clustersTaken.begin(); it != clustersTaken.end(); ++it)
            {
                system_->diskSpaceManager_.freeCluster(*it);
            }
            return TRAP;
        }
        clustersTaken.push_back(cluster);
    }
    for (auto it = clustersTaken.begin(); it != clustersTaken.end(); ++it)
    {
        system_->swapPartition_->writeCluster(*it, (const char *)content);
        content = (void *)((char *)content + PAGE_SIZE);
    }

    std::lock_guard<std::mutex> lock(mutex_guard_);
    VirtualAddress addr = startAddress;
    for (auto it = clustersTaken.begin(); it != clustersTaken.end(); ++it, addr += PAGE_SIZE)
    {
        PmtEntry1 *descr = pmt0_[VADDR_PMT0_ENTRY(addr)].pmt1 + VADDR_PMT1_ENTRY(addr);
        descr->location = *it;
        BIT_SET(descr->flags, DESC_BIT_SWAPPED);
    }

    return OK;
}

Status KernelProcess::deleteSegment(VirtualAddress startAddress)
{
    if (!IS_ALIGNED_TO_PAGE(startAddress))
    {
        return TRAP;
    }

    return removeSegment(startAddress);
}

PmtEntry1 *KernelProcess::getVictim()
{
    if (KernelProcess::clockHand == nullptr)
    {
        return nullptr;
    }

    while (BIT_IS_SET(KernelProcess::clockHand->flags, DESC_BIT_REFERENCE))
    {
        if (SHARED_SEGMENT_ID(KernelProcess::clockHand->flags) == 0)
        {
            BIT_CLEAR(KernelProcess::clockHand->flags, DESC_BIT_REFERENCE);
            KernelProcess::clockHand = KernelProcess::clockHand->next;
        }
        else // page belongs to a shared segment
        {
            VirtualAddress startAddress = SHARED_PAGE_ID(KernelProcess::clockHand->flags) << BITS_IN_VADDR_OFFSET;
            int pmt0Entry = VADDR_PMT0_ENTRY(startAddress);
            int pmt1Entry = VADDR_PMT1_ENTRY(startAddress);
            SharedSegmentDescr *sd = findSharedSegmentById(SHARED_SEGMENT_ID(KernelProcess::clockHand->flags));
            for (auto it : sd->processes_)
            {
                BIT_CLEAR(it->pmt0_[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_REFERENCE);
            }
        }
    }

    PmtEntry1 *ret = KernelProcess::clockHand;
    if (KernelProcess::clockHand == KernelProcess::clockHand->next)
    {
        KernelProcess::clockHand = nullptr;
    }
    else
    {
        KernelProcess::clockHand = KernelProcess::clockHand->next;
        ret->prev->next = ret->next;
        ret->next->prev = ret->prev;
    }

    ret->next = nullptr;
    ret->prev = nullptr;
    return ret;
}

SharedSegmentDescr *KernelProcess::findSharedSegmentById(unsigned int id)
{
    auto it = std::find_if(
        KernelProcess::sharedSegments.begin(),
        KernelProcess::sharedSegments.end(),
        [id](std::pair<std::string, SharedSegmentDescr *> e) { return e.second->id_ == id;  }
    );
    if (it == KernelProcess::sharedSegments.end())
    {
        return nullptr;
    }

    return it->second;
}

// Note: It is assmed that access() method of KernelSystem is called before a call to this method
// and returned PAGE_FAULT.
Status KernelProcess::pageFault(VirtualAddress startAddress)
{
    std::lock_guard<std::mutex> lock(mutex_guard_);

    int pmt0Entry = VADDR_PMT0_ENTRY(startAddress);
    int pmt1Entry = VADDR_PMT1_ENTRY(startAddress);
    PmtEntry1 *descr = pmt0_[pmt0Entry].pmt1 + pmt1Entry;
    PhysicalAddress frameAddress = system_->processSpaceManager_.alloc();
    bool sharedPage = false;
    SharedSegmentDescr *ssd = nullptr;

    if (SHARED_SEGMENT_ID(descr->flags) != 0)
    {
        sharedPage = true;
        ssd = findSharedSegmentById(SHARED_SEGMENT_ID(descr->flags));
        if (ssd == nullptr)
        {
            return TRAP;
        }
    }

    if (frameAddress) // new free frame, no page replacement
    {
        FrameNum frame = ((char *)frameAddress - (char *)system_->processSpace_) / FRAME_SIZE;
        if (BIT_IS_SET(descr->flags, DESC_BIT_SWAPPED))
        {
            ClusterNo locationOnDisk = descr->location;
            system_->swapPartition_->readCluster(locationOnDisk, (char *)frameAddress);
        }

        if (!sharedPage)
        {
            descr->location = frame;
        }
        else
        {
            for (auto it : ssd->processes_)
            {
                it->pmt0_[pmt0Entry].pmt1[pmt1Entry].location = frame;
            }
        }
    }
    else // page replacement
    {
        ClusterNo freeCluster;
        bool success = system_->diskSpaceManager_.takeCluster(freeCluster);
        if (!success) // make sure cluster is available for POTENTIAL swap
        {
            return TRAP;
        }

        PmtEntry1 *victim = KernelProcess::getVictim();
        if (!victim)
        {
            system_->diskSpaceManager_.freeCluster(freeCluster);
            return TRAP;
        }
        FrameNum frame = victim->location;
        PhysicalAddress frameAddress = (PhysicalAddress)((char *)system_->processSpace_ + frame * FRAME_SIZE);

        bool victimPageShared = false;
        SharedSegmentDescr *victimSsd = nullptr;
        if (SHARED_SEGMENT_ID(victim->flags) != 0)
        {
            victimPageShared = true;
            victimSsd = findSharedSegmentById(SHARED_SEGMENT_ID(victim->flags));
        }

        // if necessary, swap out victim page
        if (BIT_IS_SET(victim->flags, DESC_BIT_SWAPPED) || BIT_IS_SET(victim->flags, DESC_BIT_DIRTY))
        {
            if (!victimPageShared)
            {
                victim->location = freeCluster;
                BIT_CLEAR(victim->flags, DESC_BIT_DIRTY);
                BIT_SET(victim->flags, DESC_BIT_SWAPPED);
            }
            else // victim page belongs to a shared segment
            {
                VirtualAddress victimStartAddress = SHARED_PAGE_ID(victim->flags) << BITS_IN_VADDR_OFFSET;
                int victimPmt0Entry = VADDR_PMT0_ENTRY(victimStartAddress);
                int victimPmt1Entry = VADDR_PMT1_ENTRY(victimStartAddress);
                for (auto it : victimSsd->processes_)
                {
                    it->pmt0_[victimPmt0Entry].pmt1[victimPmt1Entry].location = freeCluster;
                    BIT_CLEAR(it->pmt0_[victimPmt0Entry].pmt1[victimPmt1Entry].flags, DESC_BIT_DIRTY);
                    BIT_SET(it->pmt0_[victimPmt0Entry].pmt1[victimPmt1Entry].flags, DESC_BIT_SWAPPED);

                }
            }

            system_->swapPartition_->writeCluster(freeCluster, (const char *)frameAddress);
        }
        else
        {
            system_->diskSpaceManager_.freeCluster(freeCluster);
        }

        // this entry is no longer mapped to frame
        if (!victimPageShared)
        {
            BIT_CLEAR(victim->flags, DESC_BIT_MAPPED);
        }
        else // victim page belongs to a shared segment
        {
            VirtualAddress victimStartAddress = SHARED_PAGE_ID(victim->flags) << BITS_IN_VADDR_OFFSET;
            int victimPmt0Entry = VADDR_PMT0_ENTRY(victimStartAddress);
            int victimPmt1Entry = VADDR_PMT1_ENTRY(victimStartAddress);
            for (auto it : victimSsd->processes_)
            {
                BIT_CLEAR(it->pmt0_[victimPmt0Entry].pmt1[victimPmt1Entry].flags, DESC_BIT_MAPPED);
            }
        }

        if (BIT_IS_SET(descr->flags, DESC_BIT_SWAPPED))
        {
            ClusterNo locationOnDisk = descr->location;
            system_->swapPartition_->readCluster(locationOnDisk, (char *)frameAddress);
        }

        if (!sharedPage)
        {
            descr->location = frame;
        }
        else
        {
            for (auto it : ssd->processes_)
            {
                it->pmt0_[pmt0Entry].pmt1[pmt1Entry].location = frame;
            }
        }
    }

    if (!sharedPage)
    {
        BIT_SET(descr->flags, DESC_BIT_MAPPED);
        BIT_SET(descr->flags, DESC_BIT_REFERENCE);
        BIT_CLEAR(descr->flags, DESC_BIT_DIRTY);
    }
    else
    {
        for (auto it : ssd->processes_)
        {
            BIT_SET(it->pmt0_[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_MAPPED);
            BIT_SET(it->pmt0_[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_REFERENCE);
            BIT_CLEAR(it->pmt0_[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_DIRTY);
        }
    }

    // link in the list for page replacement
    if (clockHand == nullptr)
    {
        clockHand = descr;
        clockHand->next = clockHand->prev = clockHand;
    }
    else
    {
        descr->prev = clockHand->prev;
        descr->next = clockHand;
        clockHand->prev->next = descr;
        clockHand->prev = descr;
    }

    return OK;
}

KernelProcess *KernelProcess::clone(ProcessId pid)
{
    std::lock_guard<std::mutex> lock(mutex_guard_);

    // allocate and initialize level 0 pmt for new process
    PmtEntry0 *pmt0 = (PmtEntry0 *)system_->pmtSpaceManager_.alloc();
    if (pmt0 == nullptr)
    {
        return nullptr;
    }

    for (int i = 0; i < PMT_0_NUM_ENTRIES; ++i)
    {
        pmt0[i].pmt1 = nullptr;
    }

    // count how many pmt1 frames and cluster is needed for the new process
    int pmt1FramesToAlloc = 0;
    int clustersToAlloc = 0;
    // also, remember all shared segments the original process is connected to
    std::vector<unsigned int> sharedSegmentIds;
    for (int i = 0; i < PMT_0_NUM_ENTRIES; ++i)
    {
        if (pmt0_[i].pmt1)
        {
            ++pmt1FramesToAlloc;
            for (int j = 0; j < PMT_1_NUM_ENTRIES; ++j)
            {
                if (!BIT_IS_SET(pmt0_[i].pmt1[j].flags, DESC_BIT_VALID))
                {
                    continue;
                }

                if (SHARED_SEGMENT_ID(pmt0_[i].pmt1[j].flags) > 0)
                {
                    // do not allocate a cluster, this is a descriptor for a shared page

                    sharedSegmentIds.push_back(SHARED_SEGMENT_ID(pmt0_[i].pmt1[j].flags));
                }
                else if (BIT_IS_SET(pmt0_[i].pmt1[j].flags, DESC_BIT_MAPPED)
                     || BIT_IS_SET(pmt0_[i].pmt1[j].flags, DESC_BIT_SWAPPED))
                {
                    ++clustersToAlloc;
                }
            }
        }
    }

    std::vector<ClusterNo> clustersTaken;
    for (int i = 0; i < clustersToAlloc; ++i)
    {
        ClusterNo cluster;
        bool success = system_->diskSpaceManager_.takeCluster(cluster);
        if (!success) // not enough clusters on disk
        {
            for (auto it = clustersTaken.begin(); it != clustersTaken.end(); ++it)
            {
                system_->diskSpaceManager_.freeCluster(*it);
            }
            system_->pmtSpaceManager_.dealloc(pmt0);
            return nullptr;
        }
        clustersTaken.push_back(cluster);
    }

    std::vector<PhysicalAddress> pmt1FramesTaken;
    for (int i = 0; i < pmt1FramesToAlloc; ++i)
    {
        PhysicalAddress pa = system_->pmtSpaceManager_.alloc();
        if (pa == nullptr)
        {
            for (auto it = clustersTaken.begin(); it != clustersTaken.end(); ++it)
            {
                system_->diskSpaceManager_.freeCluster(*it);
            }
            for (auto it = pmt1FramesTaken.begin(); it != pmt1FramesTaken.end(); ++it)
            {
                system_->pmtSpaceManager_.dealloc(*it);
            }
            system_->pmtSpaceManager_.dealloc(pmt0);
            return nullptr;
        }
        pmt1FramesTaken.push_back(pa);
    }

    char buffer[PAGE_SIZE];
    auto frameIterator = pmt1FramesTaken.begin();
    auto clusterIterator = clustersTaken.begin();

    for (int i = 0; i < PMT_0_NUM_ENTRIES; ++i)
    {
        if (pmt0_[i].pmt1)
        {
            pmt0[i].pmt1 = (PmtEntry1 *)(*frameIterator);
            ++frameIterator;

            for (int j = 0; j < PMT_1_NUM_ENTRIES; ++j)
            {
                pmt0[i].pmt1[j].flags = 0;
                pmt0[i].pmt1[j].location = 0;
                pmt0[i].pmt1[j].next = nullptr;
                pmt0[i].pmt1[j].prev = nullptr;

                if (!BIT_IS_SET(pmt0_[i].pmt1[j].flags, DESC_BIT_VALID))
                {
                    continue;
                }

                pmt0[i].pmt1[j].flags = pmt0_[i].pmt1[j].flags;

                if (SHARED_SEGMENT_ID(pmt0_[i].pmt1[j].flags) > 0)
                {
                    pmt0[i].pmt1[j].location = pmt0_[i].pmt1[j].location;
                }
                else if (BIT_IS_SET(pmt0_[i].pmt1[j].flags, DESC_BIT_MAPPED)
                     || BIT_IS_SET(pmt0_[i].pmt1[j].flags, DESC_BIT_SWAPPED))
                {
                    BIT_SET(pmt0[i].pmt1[j].flags, DESC_BIT_SWAPPED);
                    BIT_CLEAR(pmt0[i].pmt1[j].flags, DESC_BIT_MAPPED);
                    BIT_CLEAR(pmt0[i].pmt1[j].flags, DESC_BIT_DIRTY);
                    BIT_CLEAR(pmt0[i].pmt1[j].flags, DESC_BIT_REFERENCE);

                    if (BIT_IS_SET(pmt0_[i].pmt1[j].flags, DESC_BIT_MAPPED))
                    {
                        FrameNum frame = pmt0_[i].pmt1[j].location;
                        const char *frameContent = (const char *)system_->processSpace_ + frame * FRAME_SIZE;
                        memcpy(buffer, frameContent, FRAME_SIZE);
                    }
                    else
                    {
                        ClusterNo cluster = pmt0_[i].pmt1[j].location;
                        system_->swapPartition_->readCluster(cluster, buffer);
                    }

                    ClusterNo cluster = *clusterIterator;
                    ++clusterIterator;
                    system_->swapPartition_->writeCluster(cluster, buffer);

                    pmt0[i].pmt1[j].location = cluster;
                }
            }
        }
    }

    KernelProcess *newProcess = new KernelProcess(pid, pmt0, system_);

    // copy segment info
    for (auto it = segments_.begin(); it != segments_.end(); ++it)
    {
        newProcess->segments_.push_back(*it);
    }

    // add to shared segments
    for (auto it = sharedSegmentIds.begin(); it != sharedSegmentIds.end(); ++it)
    {
        SharedSegmentDescr *ssd = findSharedSegmentById(*it);
        ssd->processes_.push_back(newProcess);
    }

    return newProcess;
}

KernelProcess *KernelProcess::clone()
{
    ProcessId pid = system_->getAvailablePid();
    if (pid == (ProcessId)-1)
    {
        return nullptr;
    }
    return this->clone(pid);
}

Status KernelProcess::createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char *name, AccessType flags)
{
    if (name == nullptr)
    {
        return TRAP;
    }

    if (validateSegmentInfo(startAddress, segmentSize, flags) != OK)
    {
        return TRAP;
    }

    // lock object - sensitive operations
    std::lock_guard<std::mutex> lock(mutex_guard_);

    auto it = KernelProcess::sharedSegments.find(name);
    if (it == KernelProcess::sharedSegments.end())
    {
        // new shared segment
        if (addSharedSegment(startAddress, segmentSize, name, flags) != OK)
        {
            return TRAP;
        }
    }
    else
    {
        // shared segment already exists

        if (startAddress != it->second->startAddr_ || segmentSize != it->second->size_)
        {
            return TRAP;
        }

        if (connectToSharedSegment(it->second) != OK)
        {
            return TRAP;
        }
    }

    return OK;
}

Status KernelProcess::disconnectSharedSegment(const char *name)
{
    if (name == nullptr)
    {
        return TRAP;
    }

    // lock object - sensitive operations
    std::lock_guard<std::mutex> lock(mutex_guard_);

    auto it = KernelProcess::sharedSegments.find(name);
    if (it == KernelProcess::sharedSegments.end())
    {
        return TRAP;
    }

    KernelProcess *target = this;
    auto procPtr = std::find_if(
        it->second->processes_.begin(),
        it->second->processes_.end(),
        [target](KernelProcess *p) { return p == target; }
    );
    if (procPtr == it->second->processes_.end())
    {
        return TRAP;
    }

    bool releaseResources = it->second->processes_.size() == 1;
    SegmentDescr temp(it->second->startAddr_, it->second->size_, it->second->rights_);
    releasePmt1Entries(temp, releaseResources);
    it->second->processes_.erase(procPtr);
    return OK;
}

Status KernelProcess::deleteSharedSegment(const char *name)
{
    if (name == nullptr)
    {
        return TRAP;
    }

    auto it = KernelProcess::sharedSegments.find(name);
    if (it == KernelProcess::sharedSegments.end())
    {
        return TRAP;
    }

    for (auto p : it->second->processes_)
    {
        p->disconnectSharedSegment(name);
    }

    // delete shared segment from the system
    KernelProcess::sharedSegments.erase(name);

    return OK;
}

// Note: It is assumed that access() method of KernelSystem is called before a call to this method
// and returned OK. It is also assumed that the page isn't removed from the physical memory
// after a call to pageFault() and before a call to this method.
PhysicalAddress KernelProcess::getPhysicalAddress(VirtualAddress address)
{
    std::lock_guard<std::mutex> lock(mutex_guard_);

    PmtEntry1 *descr = pmt0_[VADDR_PMT0_ENTRY(address)].pmt1 + VADDR_PMT1_ENTRY(address);
    FrameNum frame = descr->location;
    if (SHARED_SEGMENT_ID(descr->flags) > 0)
    {
        SharedSegmentDescr *ssd = findSharedSegmentById(SHARED_SEGMENT_ID(descr->flags));
        if (ssd)
        {
            VirtualAddress startAddress = SHARED_PAGE_ID(descr->flags) << BITS_IN_VADDR_OFFSET;
            int pmt0Entry = VADDR_PMT0_ENTRY(startAddress);
            int pmt1Entry = VADDR_PMT1_ENTRY(startAddress);
            for (auto it : ssd->processes_)
            {
                BIT_SET(it->pmt0_[pmt0Entry].pmt1[pmt1Entry].flags, DESC_BIT_REFERENCE);
            }
        }
    }
    else
    {
        BIT_SET(descr->flags, DESC_BIT_REFERENCE);
    }
    PhysicalAddress frameAddress = (PhysicalAddress)((char *)system_->processSpace_ + frame * FRAME_SIZE);
    PhysicalAddress physicalAddress = (PhysicalAddress)((char *)frameAddress + VADDR_OFFSET(address));
    return physicalAddress;
}

inline bool isOverlap(const SegmentDescr &sd1, const SegmentDescr &sd2)
{
    VirtualAddress sd1EndAddr = sd1.startAddr_ + sd1.size_ * PAGE_SIZE - 1;
    VirtualAddress sd2EndAddr = sd2.startAddr_ + sd2.size_ * PAGE_SIZE - 1;
    return !(sd2.startAddr_ > sd1EndAddr || sd1.startAddr_ > sd2EndAddr);
}

inline bool isOverlap(SharedSegmentDescr *ssd1, SharedSegmentDescr *ssd2)
{
    VirtualAddress ssd1EndAddr = ssd1->startAddr_ + ssd1->size_ * PAGE_SIZE - 1;
    VirtualAddress ssd2EndAddr = ssd2->startAddr_ + ssd2->size_ * PAGE_SIZE - 1;
    return !(ssd2->startAddr_ > ssd1EndAddr || ssd1->startAddr_ > ssd2EndAddr);
}

Status KernelProcess::validateSegmentInfo(VirtualAddress startAddr, PageNum segmentSize, AccessType flags)
{
    VirtualAddress endAddress = startAddr + segmentSize * PAGE_SIZE - 1;

    // also handles the situation where segmentSize equals 0
    if ((!IS_ALIGNED_TO_PAGE(startAddr)) || (!IS_VADDR_VALID(startAddr))
        || (!IS_VADDR_VALID(endAddress)) || (endAddress < startAddr))
    {
        return TRAP;
    }

    if (!(flags == READ || flags == WRITE || flags == READ_WRITE || flags == EXECUTE))
    {
        return TRAP;
    }

    // lock object for traversal of segments_
    std::lock_guard<std::mutex> lock(mutex_guard_);

    SegmentDescr newDescr(startAddr, segmentSize, flags);
    for (const SegmentDescr &descr : segments_)
    {
        if (isOverlap(newDescr, descr))
            return TRAP;
    }

    return OK;
}

Status KernelProcess::addSegment(VirtualAddress startAddr, PageNum segmentSize, AccessType flags)
{
    // lock object - sensitive operations
    std::lock_guard<std::mutex> lock(mutex_guard_);

    SegmentDescr newSegmentDescr(startAddr, segmentSize, flags);
    if (initPmt1Entries(newSegmentDescr, 0) != OK)
    {
        return TRAP;
    }
    segments_.push_back(newSegmentDescr);
    return OK;
}

Status KernelProcess::addSharedSegment(VirtualAddress startAddr, PageNum segmentSize, const char *name, AccessType flags)
{
    unsigned int id;
    if (!KernelProcess::usedSharedSegmentIds.empty())
    {
        id = KernelProcess::usedSharedSegmentIds.top();
        KernelProcess::usedSharedSegmentIds.pop();
    }
    else
    {
        if (KernelProcess::nextUnusedSharedSegmentId == SHARED_SEGMENT_ID_LIMIT)
        {
            return TRAP;
        }
        id = KernelProcess::nextUnusedSharedSegmentId++;
    }

    SharedSegmentDescr *newSharedSegmentDescr = new SharedSegmentDescr(startAddr, segmentSize, id, name, flags);
    for (auto it : KernelProcess::sharedSegments)
    {
        if (isOverlap(newSharedSegmentDescr, it.second))
        {
            KernelProcess::usedSharedSegmentIds.push(id);
            return TRAP;
        }
    }

    SegmentDescr temp(startAddr, segmentSize, flags);
    if (initPmt1Entries(temp, id) != OK)
    {
        KernelProcess::usedSharedSegmentIds.push(id);
        return TRAP;
    }

    newSharedSegmentDescr->processes_.push_back(this); // add this process to the list of processes
    KernelProcess::sharedSegments[newSharedSegmentDescr->name_] = newSharedSegmentDescr;
    return OK;
}

Status KernelProcess::connectToSharedSegment(SharedSegmentDescr *descr)
{
    for (KernelProcess *const p : descr->processes_)
    {
        if (p == this) // if already connected to this segment
        {
            return OK;
        }
    }

    PmtEntry0 *otherPmt0 = nullptr;
    if (descr->processes_.size() > 0)
    {
        otherPmt0 = descr->processes_.front()->pmt0_;
    }

    SegmentDescr temp(descr->startAddr_, descr->size_, descr->rights_);
    if (initPmt1Entries(temp, descr->id_) != OK)
    {
        return TRAP;
    }

    if (otherPmt0) // if there was any other process connected to the segment
    {
        VirtualAddress addr = descr->startAddr_;
        for (unsigned int i = 0; i < descr->size_; ++i)
        {
            PmtEntry1 *pmt1EntryOther = otherPmt0[VADDR_PMT0_ENTRY(addr)].pmt1 + VADDR_PMT1_ENTRY(addr);
            PmtEntry1 *pmt1Entry = pmt0_[VADDR_PMT0_ENTRY(addr)].pmt1 + VADDR_PMT1_ENTRY(addr);

            if (BIT_IS_SET(pmt1EntryOther->flags, DESC_BIT_DIRTY))
            {
                BIT_SET(pmt1Entry->flags, DESC_BIT_DIRTY);
            }
            if (BIT_IS_SET(pmt1EntryOther->flags, DESC_BIT_SWAPPED))
            {
                BIT_SET(pmt1Entry->flags, DESC_BIT_SWAPPED);
            }
            if (BIT_IS_SET(pmt1EntryOther->flags, DESC_BIT_REFERENCE))
            {
                BIT_SET(pmt1Entry->flags, DESC_BIT_REFERENCE);
            }
            if (BIT_IS_SET(pmt1EntryOther->flags, DESC_BIT_MAPPED))
            {
                BIT_SET(pmt1Entry->flags, DESC_BIT_MAPPED);
            }

            addr += PAGE_SIZE;
        }
    }

    descr->processes_.push_back(this);
    return OK;
}

Status KernelProcess::removeSegment(VirtualAddress startAddr)
{
    // lock object - sensitive operations
    std::lock_guard<std::mutex> lock(mutex_guard_);

    auto it = std::find_if(
        segments_.begin(),
        segments_.end(),
        [startAddr](const SegmentDescr &s){ return s.startAddr_ == startAddr; }
        );

    if (it == segments_.end())
    {
        // no such segment found
        return TRAP;
    }

    releasePmt1Entries(*it, true);
    segments_.erase(it);
    return OK;
}

// Note: The caller has to make sure that the segment is valid before
//       a call to this function.
Status KernelProcess::initPmt1Entries(const SegmentDescr &sd, unsigned int sharedSegmentId)
{
    static PhysicalAddress frameStack[PMT_0_NUM_ENTRIES];
    static unsigned sp = 0;

    VirtualAddress endAddr = sd.startAddr_ + sd.size_ * PAGE_SIZE - 1;
    unsigned firstPmt0Entry = VADDR_PMT0_ENTRY(sd.startAddr_);
    unsigned lastPmt0Entry = VADDR_PMT0_ENTRY(endAddr);
    bool pmt1FramesAllocFailed = false;

    sp = 0;
    for (unsigned entry = firstPmt0Entry; entry <= lastPmt0Entry; ++entry)
    {
        if (!pmt0_[entry].pmt1)
        {
            PhysicalAddress pa = system_->pmtSpaceManager_.alloc();
            if (!pa)
            {
                pmt1FramesAllocFailed = true;
                break;
            }
            frameStack[sp++] = pa;
        }
    }

    if (pmt1FramesAllocFailed)
    {
        while (sp > 0)
        {
            system_->pmtSpaceManager_.dealloc(frameStack[--sp]);
        }
        return TRAP;
    }

    VirtualAddress addr = sd.startAddr_;
    for (unsigned entry = firstPmt0Entry; entry <= lastPmt0Entry; ++entry)
    {
        if (!pmt0_[entry].pmt1)
        {
            pmt0_[entry].pmt1 = (PmtEntry1 *)frameStack[--sp];
            for (int i = 0; i < PMT_1_NUM_ENTRIES; ++i)
            {
                pmt0_[entry].pmt1[i].flags = 0;
                pmt0_[entry].pmt1[i].location = 0;
                pmt0_[entry].pmt1[i].next = nullptr;
                pmt0_[entry].pmt1[i].prev = nullptr;
            }
        }

        PmtEntry1 *pmt1 = pmt0_[entry].pmt1;
        while (VADDR_PMT0_ENTRY(addr) == entry && addr < endAddr)
        {
            unsigned idx = VADDR_PMT1_ENTRY(addr);

            BIT_SET(pmt1[idx].flags, DESC_BIT_VALID);

            switch (sd.rights_)
            {
            default:
                break;
            case READ:
                BIT_SET(pmt1[idx].flags, DESC_BIT_READ);
                break;
            case WRITE:
                BIT_SET(pmt1[idx].flags, DESC_BIT_WRITE);
                break;
            case READ_WRITE:
                BIT_SET(pmt1[idx].flags, DESC_BIT_READ);
                BIT_SET(pmt1[idx].flags, DESC_BIT_WRITE);
                break;
            case EXECUTE:
                BIT_SET(pmt1[idx].flags, DESC_BIT_EXEC);
                break;
            }

            if (sharedSegmentId != 0)
            {
                ASSIGN_SHARED_SEGMENT(pmt1[idx].flags, sharedSegmentId);
                SET_SHARED_PAGE_ID(pmt1[idx].flags, addr >> BITS_IN_VADDR_OFFSET);
            }

            addr += PAGE_SIZE;
        }
    }

    return OK;
}

// returns a flag indicating if some other segment uses givem PMT1
bool KernelProcess::invalidateEntries(int pmt0Entry, int pmt1StartEntry, int pmt1EndEntry, bool releaseResources)
{
    bool entriesBeforeStartEntryInUse = false;
    for (int i = 0; i < pmt1StartEntry; ++i)
    {
        if (BIT_IS_SET(pmt0_[pmt0Entry].pmt1[i].flags, DESC_BIT_VALID))
        {
            entriesBeforeStartEntryInUse = true;
            break;
        }
    }

    bool entriesAfterEndEntryInUse = false;
    for (int i = pmt1EndEntry + 1; i < PMT_1_NUM_ENTRIES; ++i)
    {
        if (BIT_IS_SET(pmt0_[pmt0Entry].pmt1[i].flags, DESC_BIT_VALID))
        {
            entriesAfterEndEntryInUse = true;
            break;
        }
    }

    // invalidate entries starting from pmt1StartEntry, ending with pmt1EndEntry
    for (int i = pmt1StartEntry; i <= pmt1EndEntry; ++i)
    {
        PmtEntry1 *descr = pmt0_[pmt0Entry].pmt1 + i;

        if (descr->prev && descr->next)
        {
            if (SHARED_SEGMENT_ID(descr->flags) > 0 && !releaseResources)
            {
                SharedSegmentDescr *ssd = findSharedSegmentById(SHARED_SEGMENT_ID(descr->flags));
                KernelProcess *thisProc = this;
                auto procPtr = std::find_if(
                    ssd->processes_.begin(),
                    ssd->processes_.end(),
                    [thisProc](KernelProcess *p) { return p != thisProc; }
                );
                KernelProcess *replacementProc = *procPtr;
                PmtEntry1 *replacementDescr = replacementProc->pmt0_[pmt0Entry].pmt1 + i;

                if (descr->next == descr)
                {
                    replacementDescr->next = replacementDescr->prev = replacementDescr;
                    KernelProcess::clockHand = replacementDescr;
                }
                else
                {
                    replacementDescr->prev = descr->prev;
                    replacementDescr->next = descr->next;
                    descr->prev->next = replacementDescr;
                    descr->next->prev = replacementDescr;
                    if (KernelProcess::clockHand == descr)
                    {
                        KernelProcess::clockHand = replacementDescr;
                    }
                }
            }
            else
            {
                // unlink from page replacement list
                if (descr == KernelProcess::clockHand)
                {
                    KernelProcess::clockHand = KernelProcess::clockHand->next;
                    if (descr == KernelProcess::clockHand)
                    {
                        KernelProcess::clockHand = nullptr;
                    }
                }
                descr->prev->next = descr->next;
                descr->next->prev = descr->prev;

                /*KernelProcess::clockHand = KernelProcess::clockHand->next;
                if (descr == KernelProcess::clockHand)
                {
                    KernelProcess::clockHand = nullptr;
                }
                descr->prev->next = descr->next;
                descr->next->prev = descr->prev;*/
            }
        }

        if (releaseResources)
        {
            // release resources taken by the descriptor
            if (BIT_IS_SET(descr->flags, DESC_BIT_MAPPED)) // descr holds frame
            {
                FrameNum frame = descr->location;
                PhysicalAddress frameAddress = (PhysicalAddress)((char *)system_->processSpace_ + frame * FRAME_SIZE);
                system_->processSpaceManager_.dealloc(frameAddress);
            }
            else if (BIT_IS_SET(descr->flags, DESC_BIT_SWAPPED)) // descr holds cluster on disk
            {
                system_->diskSpaceManager_.freeCluster((ClusterNo)descr->location);
            }
        }

        descr->flags = 0;
        descr->location = 0;
        descr->prev = nullptr;
        descr->next = nullptr;
    }

    return entriesBeforeStartEntryInUse || entriesAfterEndEntryInUse;
}

// Note: The caller has to make sure that the segment is valid before
// a call to this function.
Status KernelProcess::releasePmt1Entries(const SegmentDescr &sd, bool releaseResources)
{
    VirtualAddress endAddr = sd.startAddr_ + sd.size_ * PAGE_SIZE - 1;
    VirtualAddress addr = sd.startAddr_;
    long descrsLeft = sd.size_;

    while (descrsLeft)
    {
        int pmt0Entry = VADDR_PMT0_ENTRY(addr);
        int pmt1StartEntry = VADDR_PMT1_ENTRY(addr);
        int pmt1EndEntry = pmt1StartEntry - 1;
        while (descrsLeft && VADDR_PMT0_ENTRY(addr) == pmt0Entry)
        {
            addr += PAGE_SIZE;
            --descrsLeft;
            ++pmt1EndEntry;
        }

        bool pmt1UsedByOtherSegment = invalidateEntries(pmt0Entry, pmt1StartEntry, pmt1EndEntry, releaseResources);

        if (!pmt1UsedByOtherSegment)
        {
            system_->pmtSpaceManager_.dealloc((PhysicalAddress)pmt0_[pmt0Entry].pmt1);
            pmt0_[pmt0Entry].pmt1 = nullptr;
        }
    }

    return OK;
}

// for testing purposes
std::ostream &operator<<(std::ostream &os, const KernelProcess &kp)
{
    os << "Process: (" << kp.pid_ << ")" << std::endl;

    os << "  Segments: " << std::endl;
    for (auto it = kp.segments_.begin(); it != kp.segments_.end(); ++it)
    {
        os << std::hex << "    " << std::setw(8) << std::setfill('0') << (*it).startAddr_ << ", "
            << std::setw(8) << std::setfill('0') << (*it).startAddr_ + (*it).size_ * PAGE_SIZE - 1 << ", "
            << std::dec << (*it).size_ << std::endl;
    }

    os << "  PMT1s: ";
    for (int i = 0; i < PMT_0_NUM_ENTRIES; ++i)
    {
        if (kp.pmt0_[i].pmt1 != nullptr)
        {
            os << i << " ";
        }
    }
    os << std::endl;
    return os;
}

