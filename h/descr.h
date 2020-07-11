// File: descr.h
// Summary: Type definitions for Level 0 and Level 1 Page Map Table entries.

// Virtual address layout:
//
// -----------------------------------------------------------------------------
// |   pmt level 0 entry    |   pmt level 1 entry   |      in-page offset      |
// -----------------------------------------------------------------------------
//          8 bits                    6 bits                   10 bits
//

#ifndef VM_EMU_DESCR_H
#define VM_EMU_DESCR_H

#include "vm_declarations.h"
#include "part.h"

// PmtEntry1 - Level 1 PMT Entry
// Level 1 PMT Entry size: 16 bytes
// alignof(PmtEntry1) == 4
// One Level 1 PMT consists of 2^6 = 64 entries
// Level 1 PMT size: 2^6 * 16B = 1KB = 1 page
//
#define PMT_1_NUM_ENTRIES 64

struct PmtEntry1 {
    unsigned int flags;                    // various flags - 4 bytes
    unsigned int location;                 // page physical location - either #frame or #cluster
    PmtEntry1 *next;                       // address of the next valid descriptor
    PmtEntry1 *prev;                       // address of the previous valid descriptor
};

// PmtEntry0 - Level 0 PMT Entry
// Level 0 PMT Entry size: 4 bytes
// alignof(PmtEntry0) == 4
// One Level 0 PMT consists of 2^8 = 256 entries
// Level 0 PMT size: 2^8 * 4B = 1KB = 1 page
//
#define PMT_0_NUM_ENTRIES 256

struct PmtEntry0 {
    PmtEntry1 *pmt1;                       // Level 1 PMT address for this entry - 4 bytes
};

#define DESC_BIT_MAPPED      0x00000001    // is page mapped to a frame (1) or not (0)
#define DESC_BIT_DIRTY       0x00000002    // is frame dirty (1) or not (0)
#define DESC_BIT_REFERENCE   0x00000004    // page reference bit
#define DESC_BIT_READ        0x00000008    // read permission bit
#define DESC_BIT_WRITE       0x00000010    // write permission bit
#define DESC_BIT_EXEC        0x00000020    // execute permission bit
#define DESC_BIT_SWAPPED     0x00000040    // is the frame swapped out (1) or not (0)
#define DESC_BIT_VALID       0x00000080    // is this pmt1 entry valid (1) or not (0)

#define FLAG_BITS_NUM 8

#define BIT_IS_SET(flags, mask) ((flags) & (mask))
#define BIT_SET(flags, mask) ((flags) |= (mask))
#define BIT_CLEAR(flags, mask) ((flags) &= ~(mask))

// shared segment support
#define SHARED_PAGE_ID_MASK 0x00003fff
#define SET_SHARED_PAGE_ID(flags, id) ((flags) |= (id << FLAG_BITS_NUM))
#define SHARED_PAGE_ID(flags) (((flags) >> FLAG_BITS_NUM) & SHARED_PAGE_ID_MASK)

#define SHARED_PAGE_ID_BITS_NUM 14

#define SHARED_SEGMENT_ID_MASK 0x000003ff
#define SHARED_SEGMENT_ID_LIMIT 1024 // 2^10
#define ASSIGN_SHARED_SEGMENT(flags, id) ((flags) |= (id << (FLAG_BITS_NUM + SHARED_PAGE_ID_BITS_NUM)))
#define SHARED_SEGMENT_ID(flags) (((flags) >> (FLAG_BITS_NUM + SHARED_PAGE_ID_BITS_NUM)) & SHARED_SEGMENT_ID_MASK)

#define SHARED_SEGMENT_ID_BITS_NUM 10

#endif // VM_EMU_DESCR_H

