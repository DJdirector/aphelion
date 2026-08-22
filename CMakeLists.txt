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

# libcurl is used by the AI provider layer (src/ai/http_client.*) to talk to
# Gemini, OpenRouter, and local Ollama instances over HTTP.
find_package(CURL REQUIRED)

# Define your main executable
add_executable(aphelion
    src/main.cpp
    src/ai/settings.cpp
    src/ai/http_client.cpp
    src/ai/gemini_provider.cpp
    src/ai/openrouter_provider.cpp
    src/ai/ollama_provider.cpp
    src/ai/provider_factory.cpp
)

# Link the header-only JSON target and libcurl
target_link_libraries(aphelion PRIVATE nlohmann_json::nlohmann_json CURL::libcurl)
