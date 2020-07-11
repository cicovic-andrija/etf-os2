// File: FrameAllocator.h
// Summary: FrameAllocator class header file.

#ifndef VM_EMU_FRAME_ALLOCATOR_H
#define VM_EMU_FRAME_ALLOCATOR_H

#include "vm_declarations.h"
#include <mutex>

// for testing purposes
#include <iostream>

class FrameAllocator {
public:
    FrameAllocator(PhysicalAddress startAddress, PageNum size);
    ~FrameAllocator();

    PhysicalAddress alloc();
    void dealloc(PhysicalAddress framePhysicalAddress);
    FrameNum freeFramesCount();
    FrameNum getFrameSpaceSize() const;
    bool isFree(FrameNum frameNum);

    // for testing purposes
    friend std::ostream &operator<<(std::ostream &os, const FrameAllocator &fa);

private:
    struct FreeSegment {
        FreeSegment *next;
        PageNum size;
    };

    FreeSegment *freeSegmentHead_;
    FrameNum freeFramesCount_;
    FrameAddress frameSpaceStartAddress_;
    FrameNum frameSpaceSize_;
    std::mutex alloc_guard_;

    bool tryMerge(FreeSegment *prev, FreeSegment *next);
};

#endif // VM_EMU_FRAME_ALLOCATOR_H

