// File: ClusterManager.cpp
// Summary: ClusterManager class implementation file.

#include "ClusterManager.h"

ClusterManager::ClusterManager(ClusterNo numOfClusters):
    numOfClusters_(numOfClusters), nextUnusedCluster_(0), freedClusters_(), cluster_guard_()
{

}

bool ClusterManager::takeCluster(ClusterNo &out_cluster)
{
    std::lock_guard<std::mutex> lock(cluster_guard_);

    if (!freedClusters_.empty())
    {
        out_cluster = freedClusters_.top();
        freedClusters_.pop();
        return true;
    }
    if (nextUnusedCluster_ < numOfClusters_)
    {
        out_cluster = nextUnusedCluster_++;
        return true;
    }
    return false; // no free clusters
}

void ClusterManager::freeCluster(ClusterNo cluster)
{
    std::lock_guard<std::mutex> lock(cluster_guard_);
    freedClusters_.push(cluster);
}

