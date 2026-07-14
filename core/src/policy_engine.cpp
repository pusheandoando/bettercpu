// core/src/policy_engine.cpp
#include "bettercpu/policy_engine.hpp"

#include <thread>





namespace bettercpu {
namespace {
constexpr int64_t kThermalPassiveDefaultMillidegrees = 85000;
constexpr int64_t kThermalRecoveryMillidegrees = 78000;
constexpr uint64_t kNvmeDefaultReadLatencyNs = 2000000;
constexpr uint64_t kNvmeDefaultWriteLatencyNs = 10000000;
constexpr double kIdleLoadThreshold = 0.05;
constexpr double kLightLoadThreshold = 0.2;
constexpr double kHeavyLoadThreshold = 0.8;
constexpr int kEppPerformance = 0;
constexpr int kEppBalanced = 128;
constexpr int kEppPowerSave = 200;
}

PolicyEngine::PolicyEngine()
    : stop_requested_(false) {
    cores_ = topology_.discover_cores();
}

void PolicyEngine::request_stop() {
    stop_requested_.store(true);
}

void PolicyEngine::apply_initial_state() {
    for (const auto& core : cores_) {
        if (!core.is_online) {
            continue;
        }

        set_governor_if_changed(core.cpu_id, "schedutil");
        uint64_t max_freq = governor_.current_max_freq_khz(core.cpu_id);
        original_max_freq_khz_[core.cpu_id] = max_freq;
        current_max_freq_khz_[core.cpu_id] = max_freq;
    }

    for (const auto& device_name : io_storage_.list_block_devices()) {
        DiskKind kind = io_storage_.detect_kind(device_name);

        if (kind == DiskKind::Nvme) {
            set_scheduler_if_changed(device_name, "none");
        } else if (kind == DiskKind::Hdd) {
            set_scheduler_if_changed(device_name, "mq-deadline");
        }
    }
}

void PolicyEngine::set_governor_if_changed(int cpu_id, const std::string& governor_name) {
    auto applied_iterator = applied_governors_.find(cpu_id);

    if (applied_iterator != applied_governors_.end() && applied_iterator->second == governor_name) {
        return;
    }

    if (governor_.set_governor(cpu_id, governor_name)) {
        applied_governors_[cpu_id] = governor_name;
    }
}

void PolicyEngine::set_epp_if_changed(int cpu_id, int epp_value) {
    auto applied_iterator = applied_epp_.find(cpu_id);

    if (applied_iterator != applied_epp_.end() && applied_iterator->second == epp_value) {
        return;
    }

    if (governor_.set_epp(cpu_id, epp_value)) {
        applied_epp_[cpu_id] = epp_value;
    }
}

void PolicyEngine::set_scheduler_if_changed(const std::string& device_name, const std::string& scheduler_name) {
    auto applied_iterator = applied_schedulers_.find(device_name);

    if (applied_iterator != applied_schedulers_.end() && applied_iterator->second == scheduler_name) {
        return;
    }

    if (io_storage_.set_scheduler(device_name, scheduler_name)) {
        applied_schedulers_[device_name] = scheduler_name;
    }
}

SystemActivityLevel PolicyEngine::classify_activity(double normalized_load, const MemoryPressureLevel& mem_level, bool thermal_event) const {
    if (thermal_event || mem_level == MemoryPressureLevel::Critical) {
        return SystemActivityLevel::Heavy;
    }

    if (mem_level == MemoryPressureLevel::Rising || normalized_load > kHeavyLoadThreshold) {
        return SystemActivityLevel::Moderate;
    }

    if (normalized_load > kIdleLoadThreshold) {
        return SystemActivityLevel::Light;
    }

    return SystemActivityLevel::Idle;
}

int PolicyEngine::compute_sleep_interval_ms(SystemActivityLevel activity_level) const {
    switch (activity_level) {
        case SystemActivityLevel::Heavy:
            return 200;
        case SystemActivityLevel::Moderate:
            return 1000;
        case SystemActivityLevel::Light:
            return 3000;
        case SystemActivityLevel::Idle:
        default:
            return 8000;
    }
}

void PolicyEngine::evaluate_thermal_policy() {
    std::vector<ThermalZoneReading> readings = sensors_.read_thermal_zones();
    bool over_threshold = false;

    for (const auto& reading : readings) {
        if (reading.temp_millidegrees > kThermalPassiveDefaultMillidegrees) {
            over_threshold = true;
            break;
        }
    }

    for (const auto& core : cores_) {
        if (!core.is_online) {
            continue;
        }

        auto original_iterator = original_max_freq_khz_.find(core.cpu_id);
        if (original_iterator == original_max_freq_khz_.end()) {
            continue;
        }

        uint64_t original_max = original_iterator->second;
        uint64_t applied_max = current_max_freq_khz_[core.cpu_id];
        uint64_t target_max = applied_max;

        if (over_threshold) {
            uint64_t reduced_max = applied_max - (applied_max / 20);
            uint64_t floor_max = original_max / 2;
            target_max = reduced_max < floor_max ? floor_max : reduced_max;
        } else {
            bool has_recovered = true;

            for (const auto& reading : readings) {
                if (reading.temp_millidegrees > kThermalRecoveryMillidegrees) {
                    has_recovered = false;
                    break;
                }
            }

            if (has_recovered && applied_max < original_max) {
                target_max = original_max;
            }
        }

        if (target_max != applied_max) {
            if (governor_.set_max_freq_khz(core.cpu_id, target_max)) {
                current_max_freq_khz_[core.cpu_id] = target_max;
            }
        }
    }
}

void PolicyEngine::evaluate_load_policy(SystemActivityLevel activity_level) {
    LoadAverage load = sensors_.read_load_average();
    int online_cores = topology_.online_cpu_count();
    if (online_cores == 0) {
        return;
    }

    double normalized_load = load.avg1 / static_cast<double>(online_cores);
    int target_epp = kEppBalanced;

    if (activity_level == SystemActivityLevel::Idle) {
        target_epp = kEppPowerSave;
    } else if (normalized_load > kHeavyLoadThreshold) {
        target_epp = kEppPerformance;
    } else if (normalized_load < kLightLoadThreshold) {
        target_epp = kEppPowerSave;
    }

    for (const auto& core : cores_) {
        if (!core.is_online) {
            continue;
        }

        set_epp_if_changed(core.cpu_id, target_epp);
    }
}

void PolicyEngine::evaluate_memory_policy() {
    MemoryPressure pressure = mem_guard_.read_memory_pressure();
    MemoryPressureLevel level = mem_guard_.classify(pressure);
    
    if (level == MemoryPressureLevel::Healthy) {
        return;
    }
    
    if (level == MemoryPressureLevel::Rising) {
        int current_pressure = mem_guard_.read_vfs_cache_pressure();
        mem_guard_.set_vfs_cache_pressure(current_pressure + 20);
        io_storage_.set_dirty_ratio(io_storage_.read_dirty_ratio() > 5 ? io_storage_.read_dirty_ratio() - 5 : 5);
        return;
    }

    std::vector<ProcessMemoryUsage> top_consumers = mem_guard_.top_memory_consumers(1);
    if (!top_consumers.empty()) {
        mem_guard_.request_graceful_termination(top_consumers.front().pid);
    }
}

void PolicyEngine::evaluate_io_policy(SystemActivityLevel activity_level) {
    if (activity_level == SystemActivityLevel::Idle) {
        return;
    }

    for (const auto& device_name : io_storage_.list_block_devices()) {
        if (io_storage_.detect_kind(device_name) != DiskKind::Nvme) {
            continue;
        }

        DiskStatsSample sample = io_storage_.read_diskstats(device_name);

        if (sample.io_in_progress > 4) {
            set_scheduler_if_changed(device_name, "mq-deadline");
        } else {
            set_scheduler_if_changed(device_name, "none");
            io_storage_.set_nvme_latency_target(device_name, kNvmeDefaultReadLatencyNs, kNvmeDefaultWriteLatencyNs);
        }
    }
}

void PolicyEngine::tick() {
    MemoryPressure pressure = mem_guard_.read_memory_pressure();
    MemoryPressureLevel level = mem_guard_.classify(pressure);
    std::vector<ThermalZoneReading> readings = sensors_.read_thermal_zones();
    bool thermal_event = false;

    for (const auto& reading : readings) {
        if (reading.temp_millidegrees > kThermalPassiveDefaultMillidegrees) {
            thermal_event = true;
            break;
        }
    }

    LoadAverage load = sensors_.read_load_average();
    int online_cores = topology_.online_cpu_count();
    double normalized_load = online_cores > 0 ? load.avg1 / static_cast<double>(online_cores) : 0.0;
    SystemActivityLevel activity_level = classify_activity(normalized_load, level, thermal_event);

    evaluate_thermal_policy();
    evaluate_load_policy(activity_level);
    evaluate_memory_policy();
    evaluate_io_policy(activity_level);

    int sleep_ms = compute_sleep_interval_ms(activity_level);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
}

void PolicyEngine::run() {
    apply_initial_state();

    while (!stop_requested_.load()) {
        tick();
    }
}
}