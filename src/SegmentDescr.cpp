// File: SegmentDescr.cpp
// Summary: SegmentDescr struct implementation.

#include "SegmentDescr.h"

SegmentDescr::SegmentDescr(VirtualAddress virtualAddress, PageNum size, AccessType accessRights) :
    startAddr_(virtualAddress), size_(size), rights_(accessRights)
{

}

