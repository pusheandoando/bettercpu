# cli/CMakeLists.txt
add_executable(bettercpu
    src/main.cpp
)
target_link_libraries(bettercpu PRIVATE bettercpu_core)
target_compile_definitions(bettercpu PRIVATE BETTERCPU_VERSION="${BETTERCPU_VERSION_STRING}")
bettercpu_apply_compiler_options(bettercpu)