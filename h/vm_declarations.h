// File: vm_declarations.h
// Summary: General type declarations and constant definitions.
//
// Note: Targeted architecture: x86 (view readme file for more details).

#ifndef VM_EMU_VM_DECLARATIONS_H
#define VM_EMU_VM_DECLARATIONS_H

// Virtual address layout:
//
// -----------------------------------------------------------------------------
// |   pmt level 0 entry    |   pmt level 1 entry   |      in-page offset      |
// -----------------------------------------------------------------------------
//          8 bits                    6 bits                   10 bits
//

// virtual address length in bits
#define BITS_IN_VADDR 24

// offset part of virtual address
#define BITS_IN_VADDR_OFFSET 10

// pmt1 entry part of virtual address
#define BITS_IN_VADDR_PMT1_ENTRY 6

// pmt0 entry part of virtual address
#define BITS_IN_VADDR_PMT0_ENTRY 8

// virtual address mask
#define VADDR_MASK (~(~0L << BITS_IN_VADDR))

// virtual address offset mask
#define VADDR_OFFSET_MASK (~(~0L << BITS_IN_VADDR_OFFSET))

// virtual address pmt1 entry mask
#define VADDR_PMT1_ENTRY_MASK (~(~0L << BITS_IN_VADDR_PMT1_ENTRY))

// virtual address pmt0 entry mask
#define VADDR_PMT0_ENTRY_MASK (~(~0L << BITS_IN_VADDR_PMT0_ENTRY))

// is virtual address valid
#define IS_VADDR_VALID(vaddr) (!(~VADDR_MASK & vaddr))

// is virtual address aligned to the start of the page
#define IS_ALIGNED_TO_PAGE(vaddr) (!(VADDR_OFFSET_MASK & vaddr))

// virtual address offset
#define VADDR_OFFSET(vaddr) (vaddr & VADDR_OFFSET_MASK)

// virtual address pmt1 entry
#define VADDR_PMT1_ENTRY(vaddr) \
((vaddr >> BITS_IN_VADDR_OFFSET) & VADDR_PMT1_ENTRY_MASK)

// virtual address pmt0 entry
#define VADDR_PMT0_ENTRY(vaddr) \
((vaddr >> (BITS_IN_VADDR_OFFSET + BITS_IN_VADDR_PMT1_ENTRY)) \
 & VADDR_PMT0_ENTRY_MASK)

// Page number - 32 bits (4 bytes)
typedef unsigned long PageNum;

// Virtual address - 32 bits (4 bytes)
typedef unsigned long VirtualAddress;

// Physical address - 32 bits (4 bytes)
typedef void *PhysicalAddress;

// Time - 32 bits (4 bytes)
typedef unsigned long Time;

// Process id - no more than 32 bits
typedef unsigned ProcessId;

// Address translation operation status
enum Status {OK, PAGE_FAULT, TRAP, ACCESS_VIOLATION};

// Page access type
enum AccessType {READ, WRITE, READ_WRITE, EXECUTE};

// Page size - 1024B or 1KB
#define PAGE_SIZE 1024

// Frame number - same as the page number
typedef PageNum FrameNum;

// Frame size - same as page size
#define FRAME_SIZE PAGE_SIZE

// Frame address
typedef char (*FrameAddress)[FRAME_SIZE];

#endif // VM_EMU_VM_DECLARATIONS_H

