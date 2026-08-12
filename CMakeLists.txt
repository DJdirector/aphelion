cmake_minimum_required(VERSION 3.15)
project(aphelion LANGUAGES CXX)

# Set C++ Standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define executable target and source file
add_executable(aphelion src/main.cpp)
