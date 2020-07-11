// File: SharedSegmentDescr.h
// Summary: SharedSegmentDescr struct definition.

#ifndef VM_EMU_SHARED_SEGMENT_DESCR_H
#define VM_EMU_SHARED_SEGMENT_DESCR_H

#include <string>
#include <vector>
#include "vm_declarations.h"

class KernelProcess;

struct SharedSegmentDescr {
    SharedSegmentDescr(VirtualAddress va, PageNum size, unsigned int id, const char *name, AccessType rights);
    unsigned int id_;
    std::string name_;
    VirtualAddress startAddr_;
    PageNum size_;
    AccessType rights_;
    std::vector<KernelProcess *> processes_;
};

#endif // VM_EMU_SHARED_SEGMENT_DESCR_H

