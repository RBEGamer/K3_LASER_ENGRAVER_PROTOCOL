cmake_minimum_required(VERSION 3.11)
project(k3_laser_api)
set(CMAKE_CXX_STANDARD 14)
add_executable(k3_laser_api main.cpp ./serial/serialib.cpp ./bitmap/bitmap_image.hpp win.hpp win.cpp)

