// core/src/topology.cpp
#include "bettercpu/topology.hpp"

#include <fstream>
#include <sstream>
#include <dirent.h>





namespace bettercpu {
uint32_t Topology::read_cpu_capacity(int cpu_id) const {
    std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu_id) + "/cpu_capacity";
    std::ifstream capacity_file(path);
    
    if (!capacity_file.is_open()) {
        return 1024;
    }
    
    uint32_t capacity = 1024;
    
    capacity_file >> capacity;
    
    return capacity;
}

int Topology::read_core_id(int cpu_id) const {
    std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu_id) + "/topology/core_id";
    std::ifstream core_file(path);
    
    if (!core_file.is_open()) {
        return -1;
    }
    
    int core_id = -1;
    
    core_file >> core_id;
    
    return core_id;
}

int Topology::read_package_id(int cpu_id) const {
    std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu_id) + "/topology/physical_package_id";
    std::ifstream package_file(path);
    
    if (!package_file.is_open()) {
        return -1;
    }
    
    int package_id = -1;
    
    package_file >> package_id;
    
    return package_id;
}

bool Topology::read_online_state(int cpu_id) const {
    if (cpu_id == 0) {
        return true;
    }

    std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu_id) + "/online";
    std::ifstream online_file(path);
    
    if (!online_file.is_open()) {
        return true;
    }
    
    int online_flag = 1;
    
    online_file >> online_flag;
    
    return online_flag == 1;
}

std::vector<int> Topology::read_thread_siblings(int cpu_id) const {
    std::vector<int> siblings;
    std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu_id) + "/topology/thread_siblings_list";
    std::ifstream siblings_file(path);
    
    if (!siblings_file.is_open()) {
        return siblings;
    }
    
    std::string content;
    std::getline(siblings_file, content);
    std::istringstream stream(content);
    std::string token;
    
    while (std::getline(stream, token, ',')) {
        siblings.push_back(std::stoi(token));
    }
    
    return siblings;
}

std::vector<CpuCoreInfo> Topology::discover_cores() const {
    std::vector<CpuCoreInfo> cores;
    DIR* dir = opendir("/sys/devices/system/cpu");
    
    if (dir == nullptr) {
        return cores;
    }
    
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        
        if (name.rfind("cpu", 0) != 0) {
            continue;
        }
        
        std::string suffix = name.substr(3);
        if (suffix.empty() || !std::isdigit(static_cast<unsigned char>(suffix[0]))) {
            continue;
        }
        
        int cpu_id = std::stoi(suffix);

        CpuCoreInfo info{};
        info.cpu_id = cpu_id;
        info.is_online = read_online_state(cpu_id);
        info.core_id = read_core_id(cpu_id);
        info.package_id = read_package_id(cpu_id);
        info.capacity = read_cpu_capacity(cpu_id);
        std::vector<int> siblings = read_thread_siblings(cpu_id);
        info.is_smt_sibling = siblings.size() > 1 && siblings.front() != cpu_id;
        cores.push_back(info);
    }
    closedir(dir);

    return cores;
}

int Topology::online_cpu_count() const {
    int count = 0;
    
    for (const auto& core : discover_cores()) {
        if (core.is_online) {
            count++;
        }
    }
    
    return count;
}

bool Topology::has_hybrid_capacities(const std::vector<CpuCoreInfo>& cores) const {
    if (cores.empty()) {
        return false;
    }

    uint32_t first_capacity = cores.front().capacity;
    
    for (const auto& core : cores) {
        if (core.capacity != first_capacity) {
            return true;
        }
    }
    
    return false;
}
}