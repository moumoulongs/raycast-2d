cmake_minimum_required(VERSION 3.10)
project(raycast-2d)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)

# 添加包含目录
include_directories(include)

# 添加子目录，即 src 目录
add_subdirectory(src)