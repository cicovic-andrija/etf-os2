// File: test.cpp
// Summary: Test functions.

#include <iostream>
#include <cstring>
#include <cstdlib>

#include "tests.h"
#include "descr.h"
#include "part.h"
#include "Process.h"
#include "System.h"
#include "FrameAllocator.h"

void testDescriptorSize()
{
    std::cout << "PMT0 Descriptor" << std::endl;
    std::cout << "Size: " << sizeof(PmtEntry0) << "B" << std::endl;
    std::cout << "Alignment: " << alignof(PmtEntry0) << "B" << std::endl;
    std::cout << std::endl;

    std::cout << "PMT0 Table" << std::endl;
    std::cout << "Number of entries: " << PMT_0_NUM_ENTRIES << std::endl;
    std::cout << "Size: " << PMT_0_NUM_ENTRIES * sizeof(PmtEntry0) << "B" << std::endl;
    std::cout << std::endl;

    std::cout << "PMT1 Descriptor" << std::endl;
    std::cout << "Size: " << sizeof(PmtEntry1) << "B" << std::endl;
    std::cout << "Alignment: " << alignof(PmtEntry1) << "B" << std::endl;
    std::cout << std::endl;

    std::cout << "PMT1 Table" << std::endl;
    std::cout << "Number of entries: " << PMT_1_NUM_ENTRIES << std::endl;
    std::cout << "Size: " << PMT_1_NUM_ENTRIES * sizeof(PmtEntry1) << "B" << std::endl;
    std::cout << std::endl;
}

void testSegmentAllocation()
{
    std::cin.unsetf(std::ios::dec);
    std::cin.unsetf(std::ios::hex);
    std::cin.unsetf(std::ios::oct);

    char *frameSpace = new char[1024000];
    char *pmtSpace = new char[1024000];
    System system(frameSpace, 1000, pmtSpace, 1000, nullptr);
    Process *proc = system.createProcess();
    VirtualAddress vaddr;
    PageNum size;
    int o;
    Status status;

    while (1)
    {
        std::cout
            << "0. Exit\n"
            << "1. Allocate\n"
            << "2. Deallocate\n"
            << "3. Print\n"
            << "Choice: ";
        std::cin >> o;
        switch (o)
        {
        case 0:
            delete proc;
            delete[] frameSpace;
            delete[] pmtSpace;
            return;
            break;
        case 1:
            std::cout << "hexVaddr size: ";
            std::cin >> std::hex >> vaddr;
            std::cin >> std::dec >> size;
            status = proc->createSegment(vaddr, size, READ);
            if (status != OK)
            {
                std::cout << "Error" << std::endl;
            }
            else
            {
                std::cout << *proc << std::endl;
            }
            break;
        case 2:
            std::cout << "Vaddr: ";
            std::cin >> std::hex >> vaddr >> std::dec;
            proc->deleteSegment(vaddr);
            if (status != OK)
            {
                std::cout << "Error" << std::endl;
            }
            else
            {
                std::cout << *proc << std::endl;
            }
            break;
        case 3:
            std::cout << *proc << std::endl;
            break;
        default:
            std::cout << "Wrong command." << std::endl;
            break;
        }
    }
}

void testFrameAllocation()
{
    char *space = new char[1024000];
    FrameAllocator fa(space, 10);
    FrameNum frame;
    int o;

    while (1)
    {
        std::cout
            << "0. Exit\n"
            << "1. Allocate\n"
            << "2. Deallocate\n"
            << "Choice: ";
        std::cin >> o;
        switch (o)
        {
        case 0:
            delete[] space;
            return;
            break;
        case 1:
            fa.alloc();
            std::cout << fa << std::endl;
            break;
        case 2:
            std::cout << "Frame number: ";
            std::cin >> frame;
            fa.dealloc((FrameAddress)space + frame);
            std::cout << fa << std::endl;
            break;
        default:
            std::cout << "Wrong command." << std::endl;
            break;
        }
    }
}

void Test_01()
{
    char *frameSpace = new char[1024];
    char *pmtSpace = new char[1024000];

    if (((int)frameSpace % 4 != 0) || ((int)pmtSpace % 4) != 0)
    {
        exit(101);
    }

    Partition swap("p1.ini");
    System system(frameSpace, 1, pmtSpace, 1000, &swap);
    Process *proc = system.createProcess();

    proc->createSegment(0x00000000, 10, READ_WRITE);

    if (system.access(proc->getProcessId(), 0x00000000, WRITE) == PAGE_FAULT)
    {
        proc->pageFault(0x00000000);
        if (system.access(proc->getProcessId(), 0x00000000, WRITE) != OK) exit(42);
    }
    PhysicalAddress pa = proc->getPhysicalAddress(0x00000000);
    *((char *)pa) = 'A';


    if (system.access(proc->getProcessId(), 0x00000000, READ) == PAGE_FAULT)
    {
        proc->pageFault(0x00000000);
        if (system.access(proc->getProcessId(), 0x00000000, WRITE) != OK) exit(42);
    }
    pa = proc->getPhysicalAddress(0x00000000);
    std::cout << *((char *)pa) << std::endl;

    if (system.access(proc->getProcessId(), 0x00000400, WRITE) == PAGE_FAULT)
    {
        proc->pageFault(0x00000400);
        if (system.access(proc->getProcessId(), 0x00000400, WRITE) != OK) exit(42);
    }
    pa = proc->getPhysicalAddress(0x00000400);
    *((char *)pa) = 'B';

    if (system.access(proc->getProcessId(), 0x00000400, READ) == PAGE_FAULT)
    {
        proc->pageFault(0x00000400);
        if (system.access(proc->getProcessId(), 0x00000400, READ) != OK) exit(42);
    }
    pa = proc->getPhysicalAddress(0x00000400);
    std::cout << *((char *)pa) << std::endl;

    if (system.access(proc->getProcessId(), 0x00000000, READ) == PAGE_FAULT)
    {
        proc->pageFault(0x00000000);
        if (system.access(proc->getProcessId(), 0x00000000, WRITE) != OK) exit(42);
    }
    pa = proc->getPhysicalAddress(0x00000000);
    std::cout << *((char *)pa) << std::endl;
}

void Test_02()
{
    Partition *part = new Partition("p1.ini");
    std::cout << part->getNumOfClusters() << std::endl;
}

void Test_03()
{
    char *frameSpace = new char[1024];
    char *pmtSpace = new char[1024000];

    if (((int)frameSpace % 4 != 0) || ((int)pmtSpace % 4) != 0)
    {
        exit(101);
    }

    Partition swap("p1.ini");
    System system(frameSpace, 1, pmtSpace, 1000, &swap);
    Process *proc1 = system.createProcess();
    Process *proc2 = system.createProcess();
    PhysicalAddress pa;

    proc1->createSegment(0x00000000, 10, READ_WRITE);
    proc2->createSegment(0x00000000, 10, READ_WRITE);

    if (system.access(proc1->getProcessId(), 0x00000000, WRITE) == PAGE_FAULT)
    {
        proc1->pageFault(0x00000000);
        if (system.access(proc1->getProcessId(), 0x00000000, WRITE) != OK) exit(42);
    }
    pa = proc1->getPhysicalAddress(0x00000000);
    *((char *)pa) = 'A';


    if (system.access(proc2->getProcessId(), 0x00000000, WRITE) == PAGE_FAULT)
    {
        proc2->pageFault(0x00000000);
        if (system.access(proc2->getProcessId(), 0x00000000, WRITE) != OK) exit(42);
    }
    pa = proc2->getPhysicalAddress(0x00000000);
    *((char *)pa) = 'B';


    if (system.access(proc2->getProcessId(), 0x00000000, READ) == PAGE_FAULT)
    {
        proc2->pageFault(0x00000000);
        if (system.access(proc2->getProcessId(), 0x00000000, READ) != OK) exit(42);
    }
    pa = proc2->getPhysicalAddress(0x00000000);
    std::cout << *((char *)pa) << std::endl;

    if (system.access(proc1->getProcessId(), 0x00000000, READ) == PAGE_FAULT)
    {
        proc1->pageFault(0x00000000);
        if (system.access(proc1->getProcessId(), 0x00000000, READ) != OK) exit(42);
    }
    pa = proc1->getPhysicalAddress(0x00000000);
    std::cout << *((char *)pa) << std::endl;
}

void Test_04()
{
    char *frameSpace = new char[1024];
    char *pmtSpace = new char[1024000];

    if (((int)frameSpace % 4 != 0) || ((int)pmtSpace % 4) != 0)
    {
        exit(101);
    }

    Partition swap("p1.ini");
    System system(frameSpace, 1, pmtSpace, 1000, &swap);
    Process *p = system.createProcess();
    PhysicalAddress pa;

    char content[2048];
    strcpy(content, "Dajana");
    strcpy(content + 1024, "Ilic");

    if (p->loadSegment(0x00000000, 2, READ, content) == TRAP)
    {
        std::cout << "TRAP" << std::endl;
    }

    if (system.access(p->getProcessId(), 0x00000000, READ) == PAGE_FAULT)
    {
        Status status = p->pageFault(0x00000000);
        if (system.access(p->getProcessId(), 0x00000000, READ) != OK) exit(42);
    }
    pa = p->getPhysicalAddress(0x00000000);
    std::cout << (char *)pa << std::endl;

    if (system.access(p->getProcessId(), 0x00000400, READ) == PAGE_FAULT)
    {
        Status status = p->pageFault(0x00000400);
        if (system.access(p->getProcessId(), 0x00000400, READ) != OK) exit(42);
    }
    pa = p->getPhysicalAddress(0x00000400);
    std::cout << (char *)pa << std::endl;
}

void Test_05()
{
    char *frameSpace = new char[10240];
    char *pmtSpace = new char[1024000];

    if (((int)frameSpace % 4 != 0) || ((int)pmtSpace % 4) != 0)
    {
        exit(101);
    }

    Partition swap("p1.ini");
    System system(frameSpace, 10, pmtSpace, 1000, &swap);
    PhysicalAddress pa;
    const int nproc = 200;
    Process *proc[nproc] = { 0 };
    VirtualAddress addr = 0x00000000;
    for (int i = 0; i < nproc; ++i, addr += PAGE_SIZE)
    {
        proc[i] = system.createProcess();
        proc[i]->createSegment(addr, 1, READ_WRITE);
    }

    addr = 0x00000000;
    for (int i = 0; i < nproc; ++i, addr += PAGE_SIZE)
    {
        if (system.access(proc[i]->getProcessId(), addr, WRITE) == PAGE_FAULT)
        {
            Status status = proc[i]->pageFault(addr);
            if (system.access(proc[i]->getProcessId(), addr, WRITE) != OK) exit(42);
        }
        pa = proc[i]->getPhysicalAddress(addr);
        itoa(proc[i]->getProcessId(), (char *)pa, 10);
    }

    addr = 0x00000000;
    for (int i = 0; i < nproc; ++i, addr += PAGE_SIZE)
    {
        if (system.access(proc[i]->getProcessId(), addr, READ) == PAGE_FAULT)
        {
            Status status = proc[i]->pageFault(addr);
            if (system.access(proc[i]->getProcessId(), addr, READ) != OK) exit(42);
        }
        pa = proc[i]->getPhysicalAddress(addr);
        std::cout << (char *)pa << std::endl;
    }

}

void Test_06()
{
    char *frameSpace = new char[10240 / 2];
    char *pmtSpace = new char[1024000];

    if (((int)frameSpace % 4 != 0) || ((int)pmtSpace % 4) != 0)
    {
        exit(101);
    }

    Partition swap("p1.ini");
    System system(frameSpace, 5, pmtSpace, 1000, &swap);
    PhysicalAddress pa;
    Process *p = system.createProcess();

    p->createSegment(0x0000f800, 10, READ_WRITE);

    VirtualAddress addr = 0x0000f800;
    for (int i = 0; i < 10; ++i, addr += PAGE_SIZE)
    {
        if (system.access(p->getProcessId(), addr, WRITE) == PAGE_FAULT)
        {
            Status status = p->pageFault(addr);
            if (system.access(p->getProcessId(), addr, WRITE) != OK) exit(42);
        }
        pa = p->getPhysicalAddress(addr);
        *(char *)pa = 'A' + i;
    }

    addr = 0x0000f800;
    Process *clone = system.cloneProcess(p->getProcessId());
    for (int i = 0; i < 10; ++i, addr += PAGE_SIZE)
    {
        if (system.access(clone->getProcessId(), addr, READ) == PAGE_FAULT) {
            Status status = clone->pageFault(addr);
            if (system.access(clone->getProcessId(), addr, WRITE) != OK) exit(42);
        }
        pa = clone->getPhysicalAddress(addr);
        std::cout << *(char *)pa << std::endl;
    }

    addr = 0x0000f800;
    Process *clone2 = p->clone(0);
    for (int i = 0; i < 10; ++i, addr += PAGE_SIZE)
    {
        if (system.access(clone2->getProcessId(), addr, READ) == PAGE_FAULT) {
            Status status = clone2->pageFault(addr);
            if (system.access(clone2->getProcessId(), addr, WRITE) != OK) exit(42);
        }
        pa = clone2->getPhysicalAddress(addr);
        std::cout << *(char *)pa << std::endl;
    }

}

