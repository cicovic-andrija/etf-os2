// File: part.h
// Summary: Partition class header file. Represents the abstraction
//          used to access the partitions on hard disk drives.
//
// Instructions: Initialize a partition with a name of a configuration
//               text file. The first line of a configuration file should
//               contain the name of a file that will simulate the hard
//               drive (it will not create it in case it doesn't exist).
//               The second line of a configuration file should contain
//               the number of clusters on the partition (size of the part.)
//               If the size of a file that will simulate the hard drive
//               is smaller than the specified size in clusters, then the
//               size of a file will be extended.
// Note: View readme file for more details about part.lib library.

#ifndef VM_EMU_PART_H
#define VM_EMU_PART_H

typedef unsigned long ClusterNo;
const unsigned long ClusterSize = 1024;

class PartitionImpl;

class Partition {
public:
    Partition(const char *);

    virtual ClusterNo getNumOfClusters() const;
    virtual int readCluster(ClusterNo, char *buffer);
    virtual int writeCluster(ClusterNo, const char *buffer);

    virtual ~Partition();

private:
    PartitionImpl *myImpl;
};

#endif // VM_EMU_PART_H

