// core/src/state_backup.cpp
#include "bettercpu/state_backup.hpp"

#include <fstream>
#include <sstream>





namespace bettercpu {
StateBackup::StateBackup(std::string backup_file_path)
    : backup_file_path_(std::move(backup_file_path)) {
}

bool StateBackup::save(const SystemStateBackup& state) const {
    std::ofstream file(backup_file_path_);
    
    if (!file.is_open()) {
        return false;
    }
    
    file << state.swappiness << "\n";
    file << state.vfs_cache_pressure << "\n";
    file << state.dirty_ratio << "\n";
    file << state.dirty_background_ratio << "\n";
    file << state.cpu_freq_states.size() << "\n";
    
    for (const auto& cpu_state : state.cpu_freq_states) {
        file << cpu_state.cpu_id << " "
            << cpu_state.governor << " "
            << cpu_state.min_freq_khz << " "
            << cpu_state.max_freq_khz << " "
            << (cpu_state.epp_value.has_value() ? std::to_string(cpu_state.epp_value.value()) : "none")
            << "\n";
    }
    
    file << state.io_schedulers.size() << "\n";
    
    for (const auto& scheduler_entry : state.io_schedulers) {
        file << scheduler_entry.first << " " << scheduler_entry.second << "\n";
    }

    return file.good();
}

std::optional<SystemStateBackup> StateBackup::load() const {
    std::ifstream file(backup_file_path_);
    
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    SystemStateBackup state{};
    file >> state.swappiness >> state.vfs_cache_pressure
        >> state.dirty_ratio >> state.dirty_background_ratio;
    size_t cpu_state_count = 0;
    file >> cpu_state_count;
    
    for (size_t i = 0; i < cpu_state_count; i++) {
        CpuFreqState cpu_state{};
        std::string epp_token;
        file >> cpu_state.cpu_id >> cpu_state.governor
            >> cpu_state.min_freq_khz >> cpu_state.max_freq_khz >> epp_token;
        
        if (epp_token != "none") {
            cpu_state.epp_value = std::stoi(epp_token);
        }

        state.cpu_freq_states.push_back(cpu_state);
    }

    size_t io_scheduler_count = 0;
    file >> io_scheduler_count;
    file.ignore();
    
    for (size_t i = 0; i < io_scheduler_count; i++) {
        std::string device_name;
        std::string scheduler_name;
        file >> device_name >> scheduler_name;
        state.io_schedulers.emplace_back(device_name, scheduler_name);
    }

    return state;
}

bool StateBackup::exists() const {
    std::ifstream file(backup_file_path_);
    return file.good();
}

bool StateBackup::remove() const {
    return std::remove(backup_file_path_.c_str()) == 0;
}
}