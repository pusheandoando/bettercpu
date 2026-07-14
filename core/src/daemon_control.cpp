// core/src/daemon_control.cpp
#include "bettercpu/daemon_control.hpp"

#include <csignal>
#include <fcntl.h>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>





namespace bettercpu {
DaemonControl::DaemonControl(std::string pid_file_path)
    : pid_file_path_(std::move(pid_file_path)) {
}

pid_t DaemonControl::read_pid() const {
    std::ifstream file(pid_file_path_);
    
    if (!file.is_open()) {
        return -1;
    }
    
    pid_t pid = -1;
    file >> pid;
    
    return pid;
}

bool DaemonControl::is_running() const {
    pid_t pid = read_pid();
    
    if (pid <= 0) {
        return false;
    }

    return kill(pid, 0) == 0;
}

bool DaemonControl::write_pid_file() const {
    std::ofstream file(pid_file_path_);
    
    if (!file.is_open()) {
        return false;
    }
    
    file << getpid();
    
    return file.good();
}

bool DaemonControl::remove_pid_file() const {
    return std::remove(pid_file_path_.c_str()) == 0;
}

bool DaemonControl::send_stop_signal() const {
    pid_t pid = read_pid();
    
    if (pid <= 0) {
        return false;
    }
    
    return kill(pid, SIGTERM) == 0;
}

bool DaemonControl::daemonize() const {
    pid_t first_fork_pid = fork();
    
    if (first_fork_pid < 0) {
        return false;
    }
    
    if (first_fork_pid > 0) {
        _exit(0);
    }
    
    if (setsid() < 0) {
        return false;
    }
    
    pid_t second_fork_pid = fork();
    
    if (second_fork_pid < 0) {
        return false;
    }
    
    if (second_fork_pid > 0) {
        _exit(0);
    }
    
    umask(0);
    
    if (chdir("/") != 0) {
        return false;
    }
    
    int null_fd = open("/dev/null", O_RDWR);
    
    if (null_fd < 0) {
        return false;
    }
    
    dup2(null_fd, STDIN_FILENO);
    dup2(null_fd, STDOUT_FILENO);
    dup2(null_fd, STDERR_FILENO);
    
    if (null_fd > STDERR_FILENO) {
        close(null_fd);
    }
    
    return true;
}
}