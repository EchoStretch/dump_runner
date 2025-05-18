cmake_minimum_required(VERSION 3.5)

project(dump_runner CXX)

add_executable(dump_runner dump_runner.cpp)
target_link_libraries(dump_runner PRIVATE SceSystemService SceUserService)
set_target_properties(dump_runner PROPERTIES OUTPUT_NAME dump_runner.elf)
