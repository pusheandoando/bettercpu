// core/include/bettercpu/iostorage.hpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>





namespace bettercpu {
enum class DiskKind {
    Nvme,
    SsdSata,
    Hdd,
    Unknown,
};

struct DiskStatsSample {
    std::string device_name;
    uint64_t read_sectors;
    uint64_t write_sectors;
    uint64_t reads_completed;
    uint64_t writes_completed;
    uint64_t io_in_progress;
    uint64_t io_time_ms;
};

class IoStorage {
public:
    std::vector<std::string> list_block_devices() const;
    DiskKind detect_kind(const std::string& device_name) const;
    std::string current_scheduler(const std::string& device_name) const;
    bool set_scheduler(const std::string& device_name, const std::string& scheduler) const;
    DiskStatsSample read_diskstats(const std::string& device_name) const;
    bool set_nvme_latency_target(const std::string& device_name, uint64_t read_lat_ns, uint64_t write_lat_ns) const;
    int read_dirty_ratio() const;
    int read_dirty_background_ratio() const;
    bool set_dirty_ratio(int ratio) const;
    bool set_dirty_background_ratio(int ratio) const;
};
}