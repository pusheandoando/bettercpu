// core/src/sensors.cpp
#include "bettercpu/sensors.hpp"

#include <fstream>
#include <sstream>
#include <dirent.h>





namespace bettercpu {
std::vector<std::string> Sensors::list_thermal_zones() const {
    std::vector<std::string> zones;
    DIR* dir = opendir("/sys/class/thermal");
    
    if (dir == nullptr) {
        return zones;
    }

    struct dirent* entry;
    
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        
        if (name.rfind("thermal_zone", 0) == 0) {
            zones.push_back("/sys/class/thermal/" + name);
        }
    }

    closedir(dir);
    return zones;
}

std::vector<std::string> Sensors::list_hwmon_devices() const {
    std::vector<std::string> devices;
    DIR* dir = opendir("/sys/class/hwmon");
    
    if (dir == nullptr) {
        return devices;
    }
    
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        
        if (name.rfind("hwmon", 0) == 0) {
            devices.push_back("/sys/class/hwmon/" + name);
        }
    }
    closedir(dir);
    
    return devices;
}

std::vector<ThermalZoneReading> Sensors::read_thermal_zones() const {
    std::vector<ThermalZoneReading> readings;
    
    for (const auto& zone_path : list_thermal_zones()) {
        std::ifstream temp_file(zone_path + "/temp");
        std::ifstream type_file(zone_path + "/type");
        
        if (!temp_file.is_open() || !type_file.is_open()) {
            continue;
        }
        
        ThermalZoneReading reading;
        reading.zone_path = zone_path;
        temp_file >> reading.temp_millidegrees;
        std::getline(type_file, reading.type);
        readings.push_back(reading);
    }

    return readings;
}

double Sensors::read_hwmon_temp(const std::string& hwmon_path) const {
    std::ifstream temp_file(hwmon_path + "/temp1_input");
    
    if (!temp_file.is_open()) {
        return -1.0;
    }

    int64_t millidegrees = 0;
    temp_file >> millidegrees;
    
    return static_cast<double>(millidegrees) / 1000.0;
}

LoadAverage Sensors::read_load_average() const {
    LoadAverage load{};
    std::ifstream load_file("/proc/loadavg");
    
    if (!load_file.is_open()) {
        return load;
    }
    
    load_file >> load.avg1 >> load.avg5 >> load.avg15;
    
    return load;
}
}