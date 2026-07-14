# core/CMakeLists.txt
add_library(bettercpu_core STATIC
    src/sensors.cpp
    src/topology.cpp
    src/governor.cpp
    src/iostorage.cpp
    src/memguard.cpp
    src/state_backup.cpp
    src/daemon_control.cpp
    src/policy_engine.cpp
    src/process_manager.cpp
    src/systemd_persistence.cpp
)
target_include_directories(bettercpu_core PUBLIC include)
bettercpu_apply_compiler_options(bettercpu_core)