// File: SharedSegmentDescr.cpp
// Summary: SegmentDescr struct implementation.

#include "SharedSegmentDescr.h"
#include "KernelProcess.h"

SharedSegmentDescr::SharedSegmentDescr(VirtualAddress va, PageNum size,
    unsigned int id, const char *name, AccessType rights):
    startAddr_(va), size_(size), id_(id), name_(name), rights_(rights), processes_()
{

}

