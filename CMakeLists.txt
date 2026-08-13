cmake_minimum_required(VERSION 3.15)
project(aphelion LANGUAGES CXX)

# Set C++ Standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Fetch nlohmann/json automatically during build
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Define your main executable
add_executable(aphelion src/main.cpp)

# Link the header-only target (This bakes the JSON code directly into your binary)
target_link_libraries(aphelion PRIVATE nlohmann_json::nlohmann_json)
