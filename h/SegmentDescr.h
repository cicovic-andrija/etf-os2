// File: SegmentDescr.h
// Summary: SegmentDescr struct definition.

#ifndef VM_EMU_SEGMENT_DESCR_H
#define VM_EMU_SEGMENT_DESCR_H

#include "vm_declarations.h"

struct SegmentDescr {
    SegmentDescr(VirtualAddress va, PageNum size, AccessType rights);
    VirtualAddress startAddr_;
    PageNum size_;
    AccessType rights_;
};

#endif // VM_EMU_SEGMENT_DESCR_H

