// core/src/governor.cpp
#include "bettercpu/governor.hpp"

#include <fstream>
#include <sstream>





namespace bettercpu {
namespace {
std::string cpufreq_path(int cpu_id, const std::string& file) {
    return "/sys/devices/system/cpu/cpu" + std::to_string(cpu_id) + "/cpufreq/" + file;
}

bool write_value(const std::string& path, const std::string& value) {
    std::ofstream out(path);
    
    if (!out.is_open()) {
        return false;
    }
    
    out << value;
    
    return out.good();
}
}

std::string Governor::current_governor(int cpu_id) const {
    std::ifstream file(cpufreq_path(cpu_id, "scaling_governor"));
    std::string governor_name;
    
    if (file.is_open()) {
        file >> governor_name;
    }
    
    return governor_name;
}

uint64_t Governor::current_max_freq_khz(int cpu_id) const {
    std::ifstream file(cpufreq_path(cpu_id, "scaling_max_freq"));
    uint64_t freq_khz = 0;
    
    if (file.is_open()) {
        file >> freq_khz;
    }
    
    return freq_khz;
}

uint64_t Governor::current_min_freq_khz(int cpu_id) const {
    std::ifstream file(cpufreq_path(cpu_id, "scaling_min_freq"));
    uint64_t freq_khz = 0;
    
    if (file.is_open()) {
        file >> freq_khz;
    }
    
    return freq_khz;
}

std::optional<int> Governor::current_epp(int cpu_id) const {
    std::ifstream file(cpufreq_path(cpu_id, "energy_performance_preference"));
    
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    std::string epp_value;
    file >> epp_value;
    
    try {
        return std::stoi(epp_value);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool Governor::set_governor(int cpu_id, const std::string& governor) const {
    return write_value(cpufreq_path(cpu_id, "scaling_governor"), governor);
}
bool Governor::set_max_freq_khz(int cpu_id, uint64_t freq_khz) const {
    return write_value(cpufreq_path(cpu_id, "scaling_max_freq"), std::to_string(freq_khz));
}
bool Governor::set_min_freq_khz(int cpu_id, uint64_t freq_khz) const {
    return write_value(cpufreq_path(cpu_id, "scaling_min_freq"), std::to_string(freq_khz));
}
bool Governor::set_epp(int cpu_id, int epp_value) const {
    return write_value(cpufreq_path(cpu_id, "energy_performance_preference"), std::to_string(epp_value));
}

CpuFreqState Governor::snapshot(int cpu_id) const {
    CpuFreqState state{};
    state.cpu_id = cpu_id;
    state.governor = current_governor(cpu_id);
    state.min_freq_khz = current_min_freq_khz(cpu_id);
    state.max_freq_khz = current_max_freq_khz(cpu_id);
    state.epp_value = current_epp(cpu_id);
    
    return state;
}

bool Governor::restore(const CpuFreqState& state) const {
    bool success = true;

    success &= set_governor(state.cpu_id, state.governor);
    success &= set_min_freq_khz(state.cpu_id, state.min_freq_khz);
    success &= set_max_freq_khz(state.cpu_id, state.max_freq_khz);
    
    if (state.epp_value.has_value()) {
        success &= set_epp(state.cpu_id, state.epp_value.value());
    }
    
    return success;
}
}