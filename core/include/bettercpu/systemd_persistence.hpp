// core/include/bettercpu/systemd_persistence.hpp
#pragma once

#include <string>





namespace bettercpu {
class SystemdPersistence {
public:
    explicit SystemdPersistence(std::string unit_file_path);
    bool install_and_enable(const std::string& executable_path, const std::string& pid_file_path) const;
    bool disable() const;
    bool remove() const;
private:
    std::string unit_file_path_;
    std::string unit_name() const;
};
}