// File: ClusterManager.h
// Summary: ClusterManager class header file.

#ifndef VM_EMU_CLUSTER_MANAGER_H
#define VM_EMU_CLUSTER_MANAGER_H

#include <stack>
#include <mutex>
#include "part.h"

class ClusterManager {
public:
    explicit ClusterManager(ClusterNo numOfClusters);
    bool takeCluster(ClusterNo &out_cluster);
    void freeCluster(ClusterNo cluster);

private:
    ClusterNo numOfClusters_;
    ClusterNo nextUnusedCluster_;
    std::stack<ClusterNo> freedClusters_;
    std::mutex cluster_guard_;

};

#endif // VM_EMU_CLUSTER_MANAGER_H

