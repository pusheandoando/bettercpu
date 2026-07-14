// core/include/bettercpu/memguard.hpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>





namespace bettercpu {
struct MemoryPressure {
    double avg10;
    double avg60;
    double avg300;
};

struct ProcessMemoryUsage {
    pid_t pid;
    std::string comm;
    uint64_t rss_kb;
};

enum class MemoryPressureLevel {
    Healthy,
    Rising,
    Critical,
};

class MemGuard {
public:
    MemoryPressure read_memory_pressure() const;
    MemoryPressureLevel classify(const MemoryPressure& pressure) const;
    int read_swappiness() const;
    int read_vfs_cache_pressure() const;
    bool set_swappiness(int value) const;
    bool set_vfs_cache_pressure(int value) const;
    std::vector<ProcessMemoryUsage> top_memory_consumers(int limit) const;
    bool request_graceful_termination(pid_t pid) const;
};
}