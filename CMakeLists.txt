cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0048 NEW)
project(DisplayMode VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(displaymode displaymode.cpp)
target_link_libraries(displaymode X11 Xrandr)
