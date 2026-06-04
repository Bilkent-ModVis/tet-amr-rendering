cmake_minimum_required(VERSION 3.28)

project(utils)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(util STATIC MD5.h MD5.cpp file_cache.h)

target_include_directories(util PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(tf_reduce_opacity_progressive tool/tf_reduce_opacity_progressive.cpp)
target_link_libraries(tf_reduce_opacity_progressive PRIVATE imgui imgui_tfe CLI11 owl_viewer)