cmake_minimum_required(VERSION 3.5)

project(dump_runner CXX)

# Add this: Option for kstuff default (0=disabled, 1=enabled)
option(KSTUFF_DEFAULT "Default kstuff value" 0)

add_executable(dump_runner dump_runner.cpp kstuff-toggle.cpp)
target_link_libraries(dump_runner PRIVATE SceSystemService SceUserService)
set_target_properties(dump_runner PROPERTIES OUTPUT_NAME dump_runner.elf)
