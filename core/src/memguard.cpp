// core/src/memguard.cpp
#include "bettercpu/memguard.hpp"

#include <csignal>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <algorithm>





namespace bettercpu {
namespace {
double parse_avg10(const std::string& line) {
    std::istringstream stream(line);
    std::string token;
    
    while (stream >> token) {
        if (token.rfind("avg10=", 0) == 0) {
            return std::stod(token.substr(6));
        }
    }
    
    return 0.0;
}

double parse_avg60(const std::string& line) {
    std::istringstream stream(line);
    std::string token;
    
    while (stream >> token) {
        if (token.rfind("avg60=", 0) == 0) {
            return std::stod(token.substr(6));
        }
    }
    
    return 0.0;
}

double parse_avg300(const std::string& line) {
    std::istringstream stream(line);
    std::string token;
    
    while (stream >> token) {
        if (token.rfind("avg300=", 0) == 0) {
            return std::stod(token.substr(7));
        }
    }

    return 0.0;
}
}

MemoryPressure MemGuard::read_memory_pressure() const {
    MemoryPressure pressure{};
    std::ifstream file("/proc/pressure/memory");
    
    if (!file.is_open()) {
        return pressure;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("some ", 0) == 0) {
            pressure.avg10 = parse_avg10(line);
            pressure.avg60 = parse_avg60(line);
            pressure.avg300 = parse_avg300(line);
            break;
        }
    }

    return pressure;
}

MemoryPressureLevel MemGuard::classify(const MemoryPressure& pressure) const {
    if (pressure.avg10 > 20.0) {
        return MemoryPressureLevel::Critical;
    }

    if (pressure.avg10 >= 5.0) {
        return MemoryPressureLevel::Rising;
    }

    return MemoryPressureLevel::Healthy;
}

int MemGuard::read_swappiness() const {
    std::ifstream file("/proc/sys/vm/swappiness");
    int value = 60;
    
    if (file.is_open()) {
        file >> value;
    }
    
    return value;
}

int MemGuard::read_vfs_cache_pressure() const {
    std::ifstream file("/proc/sys/vm/vfs_cache_pressure");
    int value = 100;

    if (file.is_open()) {
        file >> value;
    }
    
    return value;
}

bool MemGuard::set_swappiness(int value) const {
    std::ofstream file("/proc/sys/vm/swappiness");
    
    if (!file.is_open()) {
        return false;
    }
    
    file << value;

    return file.good();
}

bool MemGuard::set_vfs_cache_pressure(int value) const {
    std::ofstream file("/proc/sys/vm/vfs_cache_pressure");
    
    if (!file.is_open()) {
        return false;
    }
    
    file << value;
    
    return file.good();
}

std::vector<ProcessMemoryUsage> MemGuard::top_memory_consumers(int limit) const {
    std::vector<ProcessMemoryUsage> results;
    DIR* proc_dir = opendir("/proc");
    
    if (proc_dir == nullptr) {
        return results;
    }
    
    struct dirent* entry;
    
    while ((entry = readdir(proc_dir)) != nullptr) {
        std::string name(entry->d_name);
        
        if (name.empty() || !std::isdigit(static_cast<unsigned char>(name[0]))) {
            continue;
        }
        
        pid_t pid = std::stoi(name);
        std::ifstream status_file("/proc/" + name + "/status");
        
        if (!status_file.is_open()) {
            continue;
        }
        
        ProcessMemoryUsage usage{};
        usage.pid = pid;
        usage.rss_kb = 0;
        std::string line;
        
        while (std::getline(status_file, line)) {
            if (line.rfind("Name:", 0) == 0) {
                usage.comm = line.substr(6);
            } else if (line.rfind("VmRSS:", 0) == 0) {
                std::istringstream stream(line.substr(6));
                stream >> usage.rss_kb;
            }
        }

        results.push_back(usage);
    }
    closedir(proc_dir);
    
    std::sort(results.begin(), results.end(), [](const ProcessMemoryUsage& a, const ProcessMemoryUsage& b) {
        return a.rss_kb > b.rss_kb;
    });
    
    if (static_cast<int>(results.size()) > limit) {
        results.resize(limit);
    }
    
    return results;
}

bool MemGuard::request_graceful_termination(pid_t pid) const {
    return kill(pid, SIGTERM) == 0;
}
}