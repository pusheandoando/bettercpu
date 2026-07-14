// core/src/iostorage.cpp
#include "bettercpu/iostorage.hpp"

#include <fstream>
#include <sstream>
#include <dirent.h>





namespace bettercpu {
std::vector<std::string> IoStorage::list_block_devices() const {
    std::vector<std::string> devices;
    DIR* dir = opendir("/sys/block");
    
    if (dir == nullptr) {
        return devices;
    }
    
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        
        if (name == "." || name == "..") {
            continue;
        }
        
        if (name.rfind("loop", 0) == 0 || name.rfind("ram", 0) == 0) {
            continue;
        }
        
        devices.push_back(name);
    }
    closedir(dir);
    
    return devices;
}

DiskKind IoStorage::detect_kind(const std::string& device_name) const {
    if (device_name.rfind("nvme", 0) == 0) {
        return DiskKind::Nvme;
    }

    std::ifstream rotational_file("/sys/block/" + device_name + "/queue/rotational");
    
    if (!rotational_file.is_open()) {
        return DiskKind::Unknown;
    }
    
    int rotational_flag = 0;
    rotational_file >> rotational_flag;

    return rotational_flag == 1 ? DiskKind::Hdd : DiskKind::SsdSata;
}

std::string IoStorage::current_scheduler(const std::string& device_name) const {
    std::ifstream file("/sys/block/" + device_name + "/queue/scheduler");

    if (!file.is_open()) {
        return "";
    }

    std::string token;
    while (file >> token) {
        if (!token.empty() && token.front() == '[') {
            return token.substr(1, token.size() - 2);
        }
    }

    return "";
}

bool IoStorage::set_scheduler(const std::string& device_name, const std::string& scheduler) const {
    std::ofstream file("/sys/block/" + device_name + "/queue/scheduler");
    
    if (!file.is_open()) {
        return false;
    }

    file << scheduler;
    
    return file.good();
}

DiskStatsSample IoStorage::read_diskstats(const std::string& device_name) const {
    DiskStatsSample sample{};
    sample.device_name = device_name;
    std::ifstream file("/proc/diskstats");
    
    if (!file.is_open()) {
        return sample;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        int major_number;
        int minor_number;
        std::string name;
        stream >> major_number >> minor_number >> name;
        
        if (name != device_name) {
            continue;
        }
        
        uint64_t reads_completed;
        uint64_t reads_merged;
        uint64_t read_sectors;
        uint64_t read_time_ms;
        uint64_t writes_completed;
        uint64_t writes_merged;
        uint64_t write_sectors;
        uint64_t write_time_ms;
        uint64_t io_in_progress;
        uint64_t io_time_ms;
        
        stream >> reads_completed >> reads_merged >> read_sectors >> read_time_ms
            >> writes_completed >> writes_merged >> write_sectors >> write_time_ms
            >> io_in_progress >> io_time_ms;
        
        sample.reads_completed = reads_completed;
        sample.read_sectors = read_sectors;
        sample.writes_completed = writes_completed;
        sample.write_sectors = write_sectors;
        sample.io_in_progress = io_in_progress;
        sample.io_time_ms = io_time_ms;

        break;
    }

    return sample;
}

bool IoStorage::set_nvme_latency_target(const std::string& device_name, uint64_t read_lat_ns, uint64_t write_lat_ns) const {
    std::string base_path = "/sys/block/" + device_name + "/queue/iosched/";
    std::ofstream read_file(base_path + "read_lat_nsec");
    std::ofstream write_file(base_path + "write_lat_nsec");
    
    if (!read_file.is_open() || !write_file.is_open()) {
        return false;
    }
    
    read_file << read_lat_ns;
    write_file << write_lat_ns;
    
    return read_file.good() && write_file.good();
}

int IoStorage::read_dirty_ratio() const {
    std::ifstream file("/proc/sys/vm/dirty_ratio");
    int ratio = 0;
    
    if (file.is_open()) {
        file >> ratio;
    }
    
    return ratio;
}

int IoStorage::read_dirty_background_ratio() const {
    std::ifstream file("/proc/sys/vm/dirty_background_ratio");
    int ratio = 0;
    
    if (file.is_open()) {
        file >> ratio;
    }
    
    return ratio;
}

bool IoStorage::set_dirty_ratio(int ratio) const {
    std::ofstream file("/proc/sys/vm/dirty_ratio");
    
    if (!file.is_open()) {
        return false;
    }
    
    file << ratio;
    
    return file.good();
}

bool IoStorage::set_dirty_background_ratio(int ratio) const {
    std::ofstream file("/proc/sys/vm/dirty_background_ratio");
    
    if (!file.is_open()) {
        return false;
    }
    
    file << ratio;
    
    return file.good();
}
}