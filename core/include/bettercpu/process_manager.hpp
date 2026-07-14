// core/include/bettercpu/process_manager.hpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>





namespace bettercpu {
struct ProcessSample {
    pid_t pid;
    std::string name;
    double cpu_percent;
};

struct ProgramGroup {
    std::string name;
    double total_cpu_percent;
    std::vector<ProcessSample> processes;
};

class ProcessManager {
public:
    std::vector<ProgramGroup> collect_programs(int sample_interval_ms) const;
    bool kill_program(const ProgramGroup& program) const;
    bool kill_process(pid_t pid) const;
private:
    struct ProcessTimes {
        std::string name;
        uint64_t total_ticks;
        bool valid;
    };
    std::vector<pid_t> list_pids() const;
    ProcessTimes read_process_times(pid_t pid) const;
};
}