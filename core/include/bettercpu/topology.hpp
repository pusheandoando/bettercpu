// core/include/bettercpu/topology.hpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>





namespace bettercpu {
struct CpuCoreInfo {
    int cpu_id;
    int core_id;
    int package_id;
    uint32_t capacity;
    bool is_online;
    bool is_smt_sibling;
};

class Topology {
public:
    std::vector<CpuCoreInfo> discover_cores() const;
    int online_cpu_count() const;
    bool has_hybrid_capacities(const std::vector<CpuCoreInfo>& cores) const;
private:
    uint32_t read_cpu_capacity(int cpu_id) const;
    int read_core_id(int cpu_id) const;
    int read_package_id(int cpu_id) const;
    bool read_online_state(int cpu_id) const;
    std::vector<int> read_thread_siblings(int cpu_id) const;
};
}