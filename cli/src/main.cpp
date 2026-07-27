// cli/src/main.cpp
#include "bettercpu/colors.hpp"
#include "bettercpu/daemon_control.hpp"
#include "bettercpu/governor.hpp"
#include "bettercpu/iostorage.hpp"
#include "bettercpu/memguard.hpp"
#include "bettercpu/policy_engine.hpp"
#include "bettercpu/process_manager.hpp"
#include "bettercpu/state_backup.hpp"
#include "bettercpu/systemd_persistence.hpp"
#include "bettercpu/topology.hpp"

#include <csignal>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <filesystem>





namespace {
constexpr const char* kVersion = BETTERCPU_VERSION;
constexpr const char* kStateDirectory = "/var/lib/bettercpu";
constexpr const char* kPidFilePath = "/var/lib/bettercpu/bettercpu.pid";
constexpr const char* kBackupFilePath = "/var/lib/bettercpu/state_backup.dat";
constexpr const char* kSystemdUnitFilePath = "/etc/systemd/system/bettercpu.service";
constexpr int kProcessSampleIntervalMs = 200;
bettercpu::PolicyEngine* g_policy_engine = nullptr;





void handle_termination_signal(int) {
    if (g_policy_engine != nullptr) {
        g_policy_engine->request_stop();
    }
}

void print_help() {
    std::cout << CLR_BOLD CLR_LGREEN "bettercpu " << kVersion << CLR_RESET " - Linux CPU Optimizer\n";
    std::cout << CLR_WHITE "Written by Christian (@pusheandoando)\n" CLR_RESET;
    std::cout << "\n";
    std::cout << CLR_BOLD "Usage:\n" CLR_RESET;
    std::cout << "  " CLR_CYAN "bettercpu -h, --help" CLR_RESET "        Show this help message\n";
    std::cout << "  " CLR_CYAN "bettercpu -v, --version" CLR_RESET "     Show the installed version\n";
    std::cout << "  " CLR_CYAN "sudo bettercpu start" CLR_RESET "        Start the adaptive tuning daemon\n";
    std::cout << "  " CLR_CYAN "sudo bettercpu stop" CLR_RESET "         Stop the daemon and restore original settings\n";
    std::cout << "  " CLR_CYAN "sudo bettercpu status" CLR_RESET "       Show whether the daemon is currently active\n";
    std::cout << "  " CLR_CYAN "sudo bettercpu clean" CLR_RESET "        Remove all bettercpu generated files\n";
    std::cout << "  " CLR_CYAN "sudo bettercpu p" CLR_RESET "            List running programs and processes by CPU usage\n";
    std::cout << "  " CLR_CYAN "sudo bettercpu p -k <id>" CLR_RESET "    Kill a program or process using its temporary id\n";
    std::cout << "  " CLR_CYAN "bettercpu p -h" CLR_RESET "              Show help for the 'p' command\n";
    std::cout << "\n";
    std::cout << CLR_WHITE "Official repo: https://github.com/pusheandoando/bettercpu\n" CLR_RESET;
}

void print_version() {
    std::cout << CLR_LWHITE "bettercpu " CLR_LGREEN << kVersion << CLR_RESET "\n";
}

bool require_root() {
    if (geteuid() != 0) {
        std::cerr << CLR_LRED "[!!] root privileges required\n" CLR_RESET;
        
        return false;
    }

    return true;
}

std::string executable_path() {
    std::error_code error_code;
    std::filesystem::path resolved_path = std::filesystem::read_symlink("/proc/self/exe", error_code);

    if (error_code) {
        return "/usr/bin/bettercpu";
    }

    return resolved_path.string();
}

bettercpu::SystemStateBackup capture_current_state(const bettercpu::Governor& governor,
    const bettercpu::MemGuard& mem_guard, const bettercpu::IoStorage& io_storage,
    const std::vector<bettercpu::CpuCoreInfo>& cores) {
    bettercpu::SystemStateBackup state{};
    
    for (const auto& core : cores) {
        if (!core.is_online) {
            continue;
        }

        state.cpu_freq_states.push_back(governor.snapshot(core.cpu_id));
    }

    state.swappiness = mem_guard.read_swappiness();
    state.vfs_cache_pressure = mem_guard.read_vfs_cache_pressure();
    state.dirty_ratio = io_storage.read_dirty_ratio();
    state.dirty_background_ratio = io_storage.read_dirty_background_ratio();
    
    for (const auto& device_name : io_storage.list_block_devices()) {
        state.io_schedulers.emplace_back(device_name, io_storage.current_scheduler(device_name));
    }

    return state;
}

int run_start_command(bool skip_confirmation, bool run_in_foreground) {
    if (!require_root()) {
        return 1;
    }

    bettercpu::DaemonControl daemon_control(kPidFilePath);
    
    if (daemon_control.is_running()) {
        std::cerr << CLR_LRED "[!!] bettercpu is already running\n" CLR_RESET;
        return 1;
    }
    
    if (!skip_confirmation) {
        std::cout << CLR_CYAN "bettercpu will start adaptive tuning for CPU, thermal, I/O, and memory.\n" CLR_RESET;
        std::cout << CLR_CYAN "Changes apply immediately, no restart required.\n" CLR_RESET;
        std::cout << CLR_WHITE "Continue? [y/n]: " CLR_RESET;

        std::string user_response;
        std::getline(std::cin, user_response);

        if (user_response != "y" && user_response != "Y") {
            std::cout << CLR_YELLOW "[!!] cancelled\n" CLR_RESET;
            return 0;
        }
    }
    
    std::filesystem::create_directories(kStateDirectory);
    bettercpu::SystemdPersistence persistence(kSystemdUnitFilePath);

    if (!persistence.install_and_enable(executable_path(), kPidFilePath)) {
        std::cerr << CLR_LRED "[!!] could not install the systemd service for boot persistence\n" CLR_RESET;
        return 1;
    }
    
    bettercpu::Topology topology;
    bettercpu::Governor governor;
    bettercpu::MemGuard mem_guard;
    bettercpu::IoStorage io_storage;
    std::vector<bettercpu::CpuCoreInfo> cores = topology.discover_cores();
    bettercpu::SystemStateBackup backup_state = capture_current_state(governor, mem_guard, io_storage, cores);
    bettercpu::StateBackup state_backup(kBackupFilePath);
    
    if (!state_backup.save(backup_state)) {
        std::cerr << CLR_LRED "[!!] could not save original system state, aborting\n" CLR_RESET;
        return 1;
    }
    
    std::cout << CLR_LGREEN "[OK] daemon started, applying adaptive tuning\n" CLR_RESET;
    std::cout.flush();

    if (run_in_foreground) {
        if (!daemon_control.write_pid_file()) {
            std::cerr << CLR_LRED "[!!] could not write the pid file\n" CLR_RESET;
            return 1;
        }
    } else {
        if (!daemon_control.daemonize()) {
            std::cerr << CLR_LRED "[!!] could not start the daemon in the background\n" CLR_RESET;
            return 1;
        }

        daemon_control.write_pid_file();
    }

    bettercpu::PolicyEngine policy_engine;
    g_policy_engine = &policy_engine;

    struct sigaction action;
    std::memset(&action, 0, sizeof(action));
    action.sa_handler = handle_termination_signal;
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);
    policy_engine.run();

    bettercpu::StateBackup restore_backup(kBackupFilePath);
    std::optional<bettercpu::SystemStateBackup> saved_state = restore_backup.load();
    
    if (saved_state.has_value()) {
        for (const auto& cpu_state : saved_state->cpu_freq_states) {
            governor.restore(cpu_state);
        }

        mem_guard.set_swappiness(saved_state->swappiness);
        mem_guard.set_vfs_cache_pressure(saved_state->vfs_cache_pressure);
        io_storage.set_dirty_ratio(saved_state->dirty_ratio);
        io_storage.set_dirty_background_ratio(saved_state->dirty_background_ratio);
        
        for (const auto& scheduler_entry : saved_state->io_schedulers) {
            io_storage.set_scheduler(scheduler_entry.first, scheduler_entry.second);
        }
    }

    daemon_control.remove_pid_file();
    
    return 0;
}

int run_stop_command() {
    if (!require_root()) {
        return 1;
    }

    bettercpu::DaemonControl daemon_control(kPidFilePath);
    
    if (!daemon_control.is_running()) {
        std::cerr << CLR_LRED "[!!] bettercpu is not running\n" CLR_RESET;
        return 1;
    }
    
    if (!daemon_control.send_stop_signal()) {
        std::cerr << CLR_LRED "[!!] could not signal the running daemon\n" CLR_RESET;
        return 1;
    }
    
    bettercpu::SystemdPersistence persistence(kSystemdUnitFilePath);
    persistence.disable();
    
    std::cout << CLR_LYELLOW "[OK] stop signal sent, original settings will be restored\n" CLR_RESET;
    
    return 0;
}

int run_status_command() {
    if (!require_root()) {
        return 1;
    }

    bettercpu::DaemonControl daemon_control(kPidFilePath);

    if (daemon_control.is_running()) {
        std::cout << CLR_LGREEN "[OK] active (pid " << daemon_control.read_pid() << ")\n" CLR_RESET;
        return 0;
    }

    std::cout << CLR_LRED "[!!] inactive\n" CLR_RESET;

    return 1;
}

int run_clean_command() {
    if (!require_root()) {
        return 1;
    }

    bettercpu::DaemonControl daemon_control(kPidFilePath);
    
    if (daemon_control.is_running()) {
        std::cerr << CLR_LRED "[!!] bettercpu is still restoring or running, run 'sudo bettercpu stop' and wait until it is inactive\n" CLR_RESET;
        return 1;
    }

    std::error_code error_code;
    std::filesystem::remove_all(kStateDirectory, error_code);
    if (error_code) {
        std::cerr << CLR_LRED "[!!] could not remove state directory: " << error_code.message() << "\n" CLR_RESET;
        return 1;
    }

    bettercpu::SystemdPersistence persistence(kSystemdUnitFilePath);
    if (!persistence.remove()) {
        std::cerr << CLR_LRED "[!!] could not remove the systemd service\n" CLR_RESET;
        return 1;
    }

    std::cout << CLR_LGREEN "[OK] all generated files removed\n" CLR_RESET;
    
    return 0;
}

std::string format_percent(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value << "%";

    return stream.str();
}

constexpr const char* kBoxHorizontal = "\u2500";
constexpr const char* kBoxTopLeft = "\u250C";
constexpr const char* kBoxTopMid = "\u252C";
constexpr const char* kBoxTopRight = "\u2510";
constexpr const char* kBoxMidLeft = "\u251C";
constexpr const char* kBoxMidMid = "\u253C";
constexpr const char* kBoxMidRight = "\u2524";
constexpr const char* kBoxBottomLeft = "\u2514";
constexpr const char* kBoxBottomMid = "\u2534";
constexpr const char* kBoxBottomRight = "\u2518";
constexpr const char* kBoxVertical = "\u2502";

struct ProcessListingRange {
    size_t start;
    size_t end;
};

std::string repeat_utf8(const std::string& unit, size_t count) {
    std::string result;
    result.reserve(unit.size() * count);

    for (size_t i = 0; i < count; i++) {
        result += unit;
    }

    return result;
}

std::string pad_right(const std::string& text, size_t width) {
    if (text.size() >= width) {
        return text;
    }

    return text + std::string(width - text.size(), ' ');
}

void print_program_process_table(const bettercpu::ProgramGroup& program) {
    const std::string pid_header = "aPID";
    const std::string process_header = "Process";
    const std::string cpu_header = "CPU";
    const std::string indent = "    ";

    size_t pid_width = pid_header.size();
    size_t process_width = process_header.size();
    size_t cpu_width = cpu_header.size();

    std::vector<std::string> pid_labels;
    std::vector<std::string> cpu_labels;

    for (size_t process_index = 0; process_index < program.processes.size(); process_index++) {
        const bettercpu::ProcessSample& process = program.processes[process_index];
        std::string pid_label = "p" + std::to_string(process_index);
        std::string cpu_label = format_percent(process.cpu_percent);

        pid_width = std::max(pid_width, pid_label.size());
        process_width = std::max(process_width, process.name.size());
        cpu_width = std::max(cpu_width, cpu_label.size());

        pid_labels.push_back(pid_label);
        cpu_labels.push_back(cpu_label);
    }

    std::string top_border = indent + kBoxTopLeft + repeat_utf8(kBoxHorizontal, pid_width + 2)
        + kBoxTopMid + repeat_utf8(kBoxHorizontal, process_width + 2)
        + kBoxTopMid + repeat_utf8(kBoxHorizontal, cpu_width + 2) + kBoxTopRight;
    std::string mid_border = indent + kBoxMidLeft + repeat_utf8(kBoxHorizontal, pid_width + 2)
        + kBoxMidMid + repeat_utf8(kBoxHorizontal, process_width + 2)
        + kBoxMidMid + repeat_utf8(kBoxHorizontal, cpu_width + 2) + kBoxMidRight;
    std::string bottom_border = indent + kBoxBottomLeft + repeat_utf8(kBoxHorizontal, pid_width + 2)
        + kBoxBottomMid + repeat_utf8(kBoxHorizontal, process_width + 2)
        + kBoxBottomMid + repeat_utf8(kBoxHorizontal, cpu_width + 2) + kBoxBottomRight;

    std::cout << CLR_CYAN << top_border << CLR_RESET "\n";
    std::cout << CLR_CYAN << indent << kBoxVertical << CLR_RESET " " << pad_right(pid_header, pid_width) << " " CLR_CYAN << kBoxVertical << CLR_RESET
        << " " << pad_right(process_header, process_width) << " " CLR_CYAN << kBoxVertical << CLR_RESET
        << " " << pad_right(cpu_header, cpu_width) << " " CLR_CYAN << kBoxVertical << CLR_RESET "\n";
    std::cout << CLR_CYAN << mid_border << CLR_RESET "\n";

    for (size_t process_index = 0; process_index < program.processes.size(); process_index++) {
        const bettercpu::ProcessSample& process = program.processes[process_index];
        std::cout << CLR_CYAN << indent << kBoxVertical << CLR_RESET " " << pad_right(pid_labels[process_index], pid_width) << " " CLR_CYAN << kBoxVertical << CLR_RESET
            << " " << pad_right(process.name, process_width) << " " CLR_CYAN << kBoxVertical << CLR_RESET
            << " " << pad_right(cpu_labels[process_index], cpu_width) << " " CLR_CYAN << kBoxVertical << CLR_RESET "\n";
    }

    std::cout << CLR_CYAN << bottom_border << CLR_RESET "\n";
}

void print_process_listing(const ProcessListingRange* range) {
    bettercpu::ProcessManager process_manager;
    std::vector<bettercpu::ProgramGroup> programs = process_manager.collect_programs(kProcessSampleIntervalMs);

    size_t display_start = 0;
    size_t display_end = programs.empty() ? 0 : programs.size() - 1;

    if (range != nullptr) {
        display_start = range->start;
        display_end = range->end;
    }

    const std::string id_header = "ID";
    const std::string program_header = "Program";
    const std::string cpu_header = "CPU";

    size_t id_width = id_header.size();
    size_t program_width = program_header.size();
    size_t cpu_width = cpu_header.size();

    std::vector<std::string> id_labels;
    std::vector<std::string> program_labels;
    std::vector<std::string> cpu_labels;

    for (size_t program_index = 0; program_index < programs.size(); program_index++) {
        if (program_index < display_start || program_index > display_end) {
            continue;
        }

        const bettercpu::ProgramGroup& program = programs[program_index];
        std::string id_label = "[" + std::to_string(program_index) + "]";
        std::string program_label = program.name + " (" + std::to_string(program.processes.size()) + ")";
        std::string cpu_label = format_percent(program.total_cpu_percent);

        id_width = std::max(id_width, id_label.size());
        program_width = std::max(program_width, program_label.size());
        cpu_width = std::max(cpu_width, cpu_label.size());

        id_labels.push_back(id_label);
        program_labels.push_back(program_label);
        cpu_labels.push_back(cpu_label);
    }

    std::string header_line = pad_right(id_header, id_width) + "  " + pad_right(program_header, program_width) + "  " + cpu_header;

    std::cout << CLR_BOLD CLR_LWHITE << header_line << CLR_RESET "\n";
    std::cout << CLR_WHITE << std::string(header_line.size(), '-') << CLR_RESET "\n";

    size_t label_index = 0;

    for (size_t program_index = 0; program_index < programs.size(); program_index++) {
        if (program_index < display_start || program_index > display_end) {
            continue;
        }

        const bettercpu::ProgramGroup& program = programs[program_index];
        std::cout << CLR_LGREEN << pad_right(id_labels[label_index], id_width) << CLR_RESET "  "
            << pad_right(program_labels[label_index], program_width) << "  "
            << CLR_LCYAN << cpu_labels[label_index] << CLR_RESET "\n";
        print_program_process_table(program);
        std::cout << "\n";
        label_index++;
    }
}

bool parse_program_id(const std::string& token, size_t& program_id) {
    if (token.empty()) {
        return false;
    }

    for (char character : token) {
        if (!std::isdigit(static_cast<unsigned char>(character))) {
            return false;
        }
    }

    program_id = static_cast<size_t>(std::stoul(token));

    return true;
}

bool parse_process_id(const std::string& token, size_t& program_id, size_t& process_id) {
    size_t separator_position = token.find('p');

    if (separator_position == std::string::npos || separator_position == 0 || separator_position == token.size() - 1) {
        return false;
    }

    std::string program_token = token.substr(0, separator_position);
    std::string process_token = token.substr(separator_position + 1);

    for (char character : program_token) {
        if (!std::isdigit(static_cast<unsigned char>(character))) {
            return false;
        }
    }

    for (char character : process_token) {
        if (!std::isdigit(static_cast<unsigned char>(character))) {
            return false;
        }
    }

    program_id = static_cast<size_t>(std::stoul(program_token));
    process_id = static_cast<size_t>(std::stoul(process_token));

    return true;
}

bool parse_listing_range(const std::string& token, ProcessListingRange& range) {
    size_t separator_position = token.find(':');

    if (separator_position == std::string::npos || separator_position == 0 || separator_position == token.size() - 1) {
        return false;
    }

    std::string start_token = token.substr(0, separator_position);
    std::string end_token = token.substr(separator_position + 1);

    for (char character : start_token) {
        if (!std::isdigit(static_cast<unsigned char>(character))) {
            return false;
        }
    }

    for (char character : end_token) {
        if (!std::isdigit(static_cast<unsigned char>(character))) {
            return false;
        }
    }

    size_t start_value = static_cast<size_t>(std::stoul(start_token));
    size_t end_value = static_cast<size_t>(std::stoul(end_token));

    if (start_value > end_value) {
        return false;
    }

    range.start = start_value;
    range.end = end_value;

    return true;
}

int run_process_kill_command(const std::string& id_token) {
    if (!require_root()) {
        return 1;
    }

    bettercpu::ProcessManager process_manager;
    std::vector<bettercpu::ProgramGroup> programs = process_manager.collect_programs(kProcessSampleIntervalMs);
    size_t program_id = 0;
    size_t process_id = 0;

    if (parse_process_id(id_token, program_id, process_id)) {
        if (program_id >= programs.size() || process_id >= programs[program_id].processes.size()) {
            std::cerr << CLR_LRED "[!!] no process found with id '" << id_token << "'\n" CLR_RESET;
            return 1;
        }

        pid_t target_pid = programs[program_id].processes[process_id].pid;

        if (!process_manager.kill_process(target_pid)) {
            std::cerr << CLR_LRED "[!!] could not terminate the requested process\n" CLR_RESET;
            return 1;
        }

        std::cout << CLR_LGREEN "[OK] process terminated\n" CLR_RESET;
        return 0;
    }

    if (parse_program_id(id_token, program_id)) {
        if (program_id >= programs.size()) {
            std::cerr << CLR_LRED "[!!] no program found with id '" << id_token << "'\n" CLR_RESET;
            return 1;
        }

        if (!process_manager.kill_program(programs[program_id])) {
            std::cerr << CLR_LRED "[!!] could not terminate the requested program\n" CLR_RESET;
            return 1;
        }

        std::cout << CLR_LGREEN "[OK] program terminated\n" CLR_RESET;
        return 0;
    }

    std::cerr << CLR_LRED "[!!] invalid id '" << id_token << "', expected format N or NpM\n" CLR_RESET;
    return 1;
}

void print_process_help() {
    std::cout << "\n" CLR_BOLD CLR_LWHITE "bettercpu p" CLR_RESET " - List and manage running programs and processes\n";
    std::cout << "\n" CLR_BOLD "Usage:\n" CLR_RESET;
    std::cout << "  " CLR_CYAN "sudo bettercpu p" CLR_RESET "                    List all running programs and processes by CPU usage\n";
    std::cout << "  " CLR_CYAN "sudo bettercpu p -l, --list" CLR_RESET "         List all running programs and processes by CPU usage\n";
    std::cout << "  " CLR_CYAN "sudo bettercpu p -k, --kill <id>" CLR_RESET "    Kill a program or process using its temporary id\n";
    std::cout << "  " CLR_CYAN "sudo bettercpu p -r, --range <a:b>" CLR_RESET "  Limit the listing to programs with id from a to b\n";
    std::cout << "  " CLR_CYAN "bettercpu p -h, --help" CLR_RESET "              Show this help message\n";
    std::cout << "\n";
}

int run_process_command(int argc, char* argv[]) {
    bool help_requested = false;
    bool kill_requested = false;
    bool range_requested = false;
    std::string kill_id;
    std::string range_token;

    for (int argument_index = 2; argument_index < argc; argument_index++) {
        std::string option(argv[argument_index]);

        if (option == "-h" || option == "--help") {
            help_requested = true;
            continue;
        }

        if (option == "-l" || option == "--list") {
            continue;
        }

        if (option == "-k" || option == "--kill") {
            if (argument_index + 1 >= argc) {
                std::cerr << CLR_LRED "[!!] missing id argument for " << option << "\n" CLR_RESET;
                return 1;
            }

            kill_requested = true;
            kill_id = argv[++argument_index];
            continue;
        }

        if (option == "-r" || option == "--range") {
            if (argument_index + 1 >= argc) {
                std::cerr << CLR_LRED "[!!] missing range argument for " << option << "\n" CLR_RESET;
                return 1;
            }

            range_requested = true;
            range_token = argv[++argument_index];
            continue;
        }

        std::cerr << CLR_LRED "[!!] unknown option '" << option << "' for command 'p'\n" CLR_RESET;
        return 1;
    }

    if (help_requested) {
        print_process_help();
        return 0;
    }

    if (kill_requested) {
        return run_process_kill_command(kill_id);
    }

    ProcessListingRange range{};

    if (range_requested) {
        if (!parse_listing_range(range_token, range)) {
            std::cerr << CLR_LRED "[!!] invalid range '" << range_token << "', expected format START:END\n" CLR_RESET;
            return 1;
        }
    }

    print_process_listing(range_requested ? &range : nullptr);

    return 0;
}
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    int command_argc = argc;
    char** command_argv = argv;
    bool launched_by_systemd = false;

    if (argc >= 2 && std::string(argv[1]) == "--boot") {
        launched_by_systemd = true;
        command_argc = argc - 1;
        command_argv = argv + 1;
    }

    if (command_argc < 2) {
        print_help();
        return 1;
    }

    std::string command(command_argv[1]);
    
    if (command == "-h" || command == "--help") {
        print_help();
        return 0;
    }
    if (command == "-v" || command == "--version") {
        print_version();
        return 0;
    }
    if (command == "start") {
        return run_start_command(launched_by_systemd, launched_by_systemd);
    }
    if (command == "stop") {
        return run_stop_command();
    }
    if (command == "status") {
        return run_status_command();
    }
    if (command == "clean") {
        return run_clean_command();
    }
    if (command == "p") {
        return run_process_command(command_argc, command_argv);
    }
    std::cerr << CLR_LRED "[!!] unknown command '" << command << "'\n" CLR_RESET;
    print_help();
    
    return 1;
}