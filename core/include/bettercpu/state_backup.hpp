// core/include/bettercpu/state_backup.hpp
#pragma once

#include "bettercpu/governor.hpp"

#include <string>
#include <vector>
#include <cstdint>





namespace bettercpu {
struct SystemStateBackup {
    std::vector<CpuFreqState> cpu_freq_states;
    int swappiness;
    int vfs_cache_pressure;
    int dirty_ratio;
    int dirty_background_ratio;
    std::vector<std::pair<std::string, std::string>> io_schedulers;
};

class StateBackup {
public:
    explicit StateBackup(std::string backup_file_path);
    bool save(const SystemStateBackup& state) const;
    std::optional<SystemStateBackup> load() const;
    bool exists() const;
    bool remove() const;
private:
    std::string backup_file_path_;
};
}