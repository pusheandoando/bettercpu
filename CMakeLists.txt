# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(bettercpu VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(cmake/CompilerOptions.cmake)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" BETTERCPU_VERSION_STRING LIMIT_COUNT 1)
add_subdirectory(core)
add_subdirectory(cli)