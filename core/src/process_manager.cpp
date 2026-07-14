// core/src/process_manager.cpp
#include "bettercpu/process_manager.hpp"

#include <csignal>
#include <chrono>
#include <cctype>
#include <thread>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <algorithm>
#include <unordered_map>





namespace bettercpu {
namespace {
constexpr int kUtimeTokenIndex = 11;
constexpr int kStimeTokenIndex = 12;
}

std::vector<pid_t> ProcessManager::list_pids() const {
    std::vector<pid_t> pids;
    DIR* proc_dir = opendir("/proc");

    if (proc_dir == nullptr) {
        return pids;
    }

    struct dirent* entry;

    while ((entry = readdir(proc_dir)) != nullptr) {
        std::string name(entry->d_name);

        if (name.empty() || !std::isdigit(static_cast<unsigned char>(name[0]))) {
            continue;
        }

        pids.push_back(static_cast<pid_t>(std::stoi(name)));
    }
    closedir(proc_dir);

    return pids;
}

ProcessManager::ProcessTimes ProcessManager::read_process_times(pid_t pid) const {
    ProcessTimes times{};
    times.valid = false;
    std::ifstream stat_file("/proc/" + std::to_string(pid) + "/stat");

    if (!stat_file.is_open()) {
        return times;
    }

    std::string line;
    std::getline(stat_file, line);
    size_t name_start = line.find('(');
    size_t name_end = line.rfind(')');

    if (name_start == std::string::npos || name_end == std::string::npos || name_end <= name_start) {
        return times;
    }

    times.name = line.substr(name_start + 1, name_end - name_start - 1);
    std::istringstream remainder(line.substr(name_end + 2));
    std::string token;
    int token_index = 0;
    uint64_t utime = 0;
    uint64_t stime = 0;

    while (remainder >> token) {
        if (token_index == kUtimeTokenIndex) {
            utime = std::stoull(token);
        } else if (token_index == kStimeTokenIndex) {
            stime = std::stoull(token);
            break;
        }

        token_index++;
    }

    times.total_ticks = utime + stime;
    times.valid = true;

    return times;
}

std::vector<ProgramGroup> ProcessManager::collect_programs(int sample_interval_ms) const {
    std::vector<pid_t> pids = list_pids();
    std::unordered_map<pid_t, ProcessTimes> first_samples;

    for (pid_t pid : pids) {
        ProcessTimes times = read_process_times(pid);

        if (times.valid) {
            first_samples[pid] = times;
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(sample_interval_ms));

    long ticks_per_second = sysconf(_SC_CLK_TCK);
    double elapsed_seconds = static_cast<double>(sample_interval_ms) / 1000.0;
    std::unordered_map<std::string, ProgramGroup> programs_by_name;

    for (const auto& sample_entry : first_samples) {
        pid_t pid = sample_entry.first;
        const ProcessTimes& first_times = sample_entry.second;
        ProcessTimes second_times = read_process_times(pid);

        if (!second_times.valid || second_times.total_ticks < first_times.total_ticks) {
            continue;
        }

        uint64_t tick_delta = second_times.total_ticks - first_times.total_ticks;
        double cpu_percent = (static_cast<double>(tick_delta) / static_cast<double>(ticks_per_second)) / elapsed_seconds * 100.0;

        ProcessSample sample{};
        sample.pid = pid;
        sample.name = second_times.name;
        sample.cpu_percent = cpu_percent;

        ProgramGroup& program = programs_by_name[second_times.name];
        program.name = second_times.name;
        program.total_cpu_percent += cpu_percent;
        program.processes.push_back(sample);
    }

    std::vector<ProgramGroup> programs;

    for (auto& program_entry : programs_by_name) {
        programs.push_back(std::move(program_entry.second));
    }

    for (auto& program : programs) {
        std::sort(program.processes.begin(), program.processes.end(), [](const ProcessSample& a, const ProcessSample& b) {
            return a.cpu_percent > b.cpu_percent;
        });
    }

    std::sort(programs.begin(), programs.end(), [](const ProgramGroup& a, const ProgramGroup& b) {
        return a.total_cpu_percent > b.total_cpu_percent;
    });

    return programs;
}

bool ProcessManager::kill_program(const ProgramGroup& program) const {
    bool success = true;

    for (const auto& process : program.processes) {
        success &= kill_process(process.pid);
    }

    return success;
}

bool ProcessManager::kill_process(pid_t pid) const {
    return kill(pid, SIGTERM) == 0;
}
}