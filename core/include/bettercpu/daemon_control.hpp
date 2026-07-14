// core/include/bettercpu/daemon_control.hpp
#pragma once

#include <string>





namespace bettercpu {
class DaemonControl {
public:
    explicit DaemonControl(std::string pid_file_path);
    bool is_running() const;
    bool write_pid_file() const;
    bool remove_pid_file() const;
    pid_t read_pid() const;
    bool send_stop_signal() const;
    bool daemonize() const;
private:
    std::string pid_file_path_;
};
}