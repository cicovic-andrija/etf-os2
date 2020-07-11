// File: FrameAllocator.cpp
// Summary: FrameAllocator class implementation file.

#include "FrameAllocator.h"

FrameAllocator::FrameAllocator(PhysicalAddress startAddress, PageNum size):
    freeSegmentHead_((FreeSegment *)startAddress),
    frameSpaceStartAddress_((FrameAddress)startAddress), frameSpaceSize_(size),
    freeFramesCount_(size), alloc_guard_()
{
    freeSegmentHead_->next = nullptr;
    freeSegmentHead_->size = size;
}

FrameAllocator::~FrameAllocator()
{

}

PhysicalAddress FrameAllocator::alloc()
{
    std::lock_guard<std::mutex> lock(alloc_guard_);

    if (freeSegmentHead_ == nullptr)
    {
        return nullptr; // exception
    }

    FreeSegment *segment = freeSegmentHead_;
    if (segment->size == 1)
    {
        freeSegmentHead_ = freeSegmentHead_->next;
    }
    --segment->size;
    --freeFramesCount_;

    // allocate the last frame of the current free segment
    FrameAddress frameAddress = (FrameAddress)segment + segment->size;
    return (PhysicalAddress)frameAddress;
}

void FrameAllocator::dealloc(PhysicalAddress framePhysicalAddress)
{
    std::lock_guard<std::mutex> lock(alloc_guard_);

    FrameAddress frameAddress = (FrameAddress)framePhysicalAddress;
    FreeSegment *newSegment = (FreeSegment *)frameAddress;
    newSegment->size = 1;

    FreeSegment *current = freeSegmentHead_;
    FreeSegment *prev = nullptr;
    while (current && (PhysicalAddress)current < (PhysicalAddress)frameAddress)
    {
        prev = current;
        current = current->next;
    }

    if ((prev && (FrameAddress)prev + prev->size > frameAddress)
        || (current && (FrameAddress)current == frameAddress))
    {
        // frame already free
        return;
    }

    newSegment->next = current;
    if (prev)
    {
        prev->next = newSegment;
    }
    else
    {
        freeSegmentHead_ = newSegment;
    }

    if (tryMerge(prev, newSegment))
    {
        newSegment = prev;
    }
    tryMerge(newSegment, current);

    ++freeFramesCount_;
}

FrameNum FrameAllocator::freeFramesCount()
{
    std::lock_guard<std::mutex> lock(alloc_guard_);

    return freeFramesCount_;
}

FrameNum FrameAllocator::getFrameSpaceSize() const
{
    return frameSpaceSize_;
}

bool FrameAllocator::isFree(FrameNum frameNum)
{
    std::lock_guard<std::mutex> lock(alloc_guard_);

    FrameAddress frameAddress = frameSpaceStartAddress_ + frameNum;
    FreeSegment *current = freeSegmentHead_;
    while (current)
    {
        if (frameAddress >= (FrameAddress)current
            && frameAddress < (FrameAddress)current + current->size)
        {
            return true;
        }
    }
    return false;
}

bool FrameAllocator::tryMerge(FreeSegment *prev, FreeSegment *next)
{
    if (!next || !prev || (FrameAddress)prev + prev->size != (FrameAddress)next)
    {
        return false;
    }

    prev->size += next->size;
    prev->next = next->next;
    return true;
}

// for testing purposes
std::ostream &operator<<(std::ostream &os, const FrameAllocator &fa)
{
    for (FrameAllocator::FreeSegment *curr = fa.freeSegmentHead_; curr; curr = curr->next)
    {
        os << (FrameAddress)curr << "(" << curr->size << ")" << std::endl;
    }
    return os;
}

