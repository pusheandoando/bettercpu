// core/src/systemd_persistence.cpp
#include "bettercpu/systemd_persistence.hpp"

#include <cstdlib>
#include <fstream>
#include <filesystem>





namespace bettercpu {
SystemdPersistence::SystemdPersistence(std::string unit_file_path)
    : unit_file_path_(std::move(unit_file_path)) {
}

std::string SystemdPersistence::unit_name() const {
    return std::filesystem::path(unit_file_path_).filename().string();
}

bool SystemdPersistence::install_and_enable(const std::string& executable_path, const std::string&) const {
    std::ofstream unit_file(unit_file_path_);

    if (!unit_file.is_open()) {
        return false;
    }

    unit_file << "[Unit]\n";
    unit_file << "Description=bettercpu adaptive tuning daemon\n";
    unit_file << "After=multi-user.target\n";
    unit_file << "\n";
    unit_file << "[Service]\n";
    unit_file << "Type=simple\n";
    unit_file << "ExecStart=" << executable_path << " --boot start\n";
    unit_file << "ExecStop=" << executable_path << " stop\n";
    unit_file << "\n";
    unit_file << "[Install]\n";
    unit_file << "WantedBy=multi-user.target\n";

    if (!unit_file.good()) {
        return false;
    }

    unit_file.close();

    if (std::system("systemctl daemon-reload") != 0) {
        return false;
    }

    std::string enable_command = "systemctl enable " + unit_name();

    return std::system(enable_command.c_str()) == 0;
}

bool SystemdPersistence::disable() const {
    std::string disable_command = "systemctl disable " + unit_name();

    return std::system(disable_command.c_str()) == 0;
}

bool SystemdPersistence::remove() const {
    std::system(("systemctl disable " + unit_name()).c_str());
    std::error_code error_code;
    std::filesystem::remove(unit_file_path_, error_code);

    if (error_code) {
        return false;
    }

    return std::system("systemctl daemon-reload") == 0;
}
}