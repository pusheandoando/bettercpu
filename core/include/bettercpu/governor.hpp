// core/include/bettercpu/governor.hpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>





namespace bettercpu {
struct CpuFreqState {
    int cpu_id;
    std::string governor;
    uint64_t min_freq_khz;
    uint64_t max_freq_khz;
    std::optional<int> epp_value;
};

class Governor {
public:
    std::string current_governor(int cpu_id) const;
    uint64_t current_max_freq_khz(int cpu_id) const;
    uint64_t current_min_freq_khz(int cpu_id) const;
    std::optional<int> current_epp(int cpu_id) const;
    bool set_governor(int cpu_id, const std::string& governor) const;
    bool set_max_freq_khz(int cpu_id, uint64_t freq_khz) const;
    bool set_min_freq_khz(int cpu_id, uint64_t freq_khz) const;
    bool set_epp(int cpu_id, int epp_value) const;
    CpuFreqState snapshot(int cpu_id) const;
    bool restore(const CpuFreqState& state) const;
};
}