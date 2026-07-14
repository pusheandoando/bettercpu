// core/include/bettercpu/sensors.hpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>





namespace bettercpu {
struct ThermalZoneReading {
    std::string zone_path;
    std::string type;
    int64_t temp_millidegrees;
};

struct LoadAverage {
    double avg1;
    double avg5;
    double avg15;
};

class Sensors {
public:
    std::vector<ThermalZoneReading> read_thermal_zones() const;
    LoadAverage read_load_average() const;
    double read_hwmon_temp(const std::string& hwmon_path) const;
private:
    std::vector<std::string> list_thermal_zones() const;
    std::vector<std::string> list_hwmon_devices() const;
};
}