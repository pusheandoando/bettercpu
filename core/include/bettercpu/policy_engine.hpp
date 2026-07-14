// core/include/bettercpu/policy_engine.hpp
#pragma once

#include "bettercpu/governor.hpp"
#include "bettercpu/iostorage.hpp"
#include "bettercpu/memguard.hpp"
#include "bettercpu/sensors.hpp"
#include "bettercpu/state_backup.hpp"
#include "bettercpu/topology.hpp"

#include <atomic>
#include <optional>
#include <unordered_map>





namespace bettercpu {
enum class SystemActivityLevel {
    Idle,
    Light,
    Moderate,
    Heavy,
};

class PolicyEngine {
public:
    PolicyEngine();
    void run();
    void request_stop();
private:
    void apply_initial_state();
    void tick();
    SystemActivityLevel classify_activity(double normalized_load, const MemoryPressureLevel& mem_level, bool thermal_event) const;
    int compute_sleep_interval_ms(SystemActivityLevel activity_level) const;
    void evaluate_thermal_policy();
    void evaluate_load_policy(SystemActivityLevel activity_level);
    void evaluate_memory_policy();
    void evaluate_io_policy(SystemActivityLevel activity_level);
    void set_governor_if_changed(int cpu_id, const std::string& governor_name);
    void set_epp_if_changed(int cpu_id, int epp_value);
    void set_scheduler_if_changed(const std::string& device_name, const std::string& scheduler_name);
    Sensors sensors_;
    Topology topology_;
    Governor governor_;
    IoStorage io_storage_;
    MemGuard mem_guard_;
    std::vector<CpuCoreInfo> cores_;
    std::unordered_map<int, uint64_t> original_max_freq_khz_;
    std::unordered_map<int, uint64_t> current_max_freq_khz_;
    std::unordered_map<int, std::string> applied_governors_;
    std::unordered_map<int, int> applied_epp_;
    std::unordered_map<std::string, std::string> applied_schedulers_;
    std::atomic<bool> stop_requested_;
};
}