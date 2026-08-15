# Aphelion

The identity of this project is still being defined. As of now, the idea is to make a free, open-source AI coding agent that runs in your terminal.

## Prerequisites
Before building this project, ensure you have the following utilities installed on your system:

* **CMake** (Version 3.15 or higher)
* **C++ Compiler** (GCC, Clang, or MSVC supporting C++17)
* **Build Tool** (Make or Ninja)

## Project Structure

```txt
aphelion/
├── CMakeLists.txt                       # Core CMake configuration script
├── README.md                            # Project documentation and setup guide
├── settings.json                        # Aphelion config file
├── src                                  # Aphelion source files
│   └── main.cpp
├── themes                               # Theme files
└── thinkers                             # Thinker files
```

## How to Build

This project uses an out-of-source build workflow to keep the workspace clean and prevent compiled binaries from being tracked by Git.

### 1. Generate the Build files

Initialize CMake to read the project rules from the root directory and prepare the compilation configuration inside a dedicated `build` folder:

```bash
cmake -S . -B build
```

### 2. Compile the Target Application

Invoke the underlying build system to compile the source code into a runnable binary.

```bash
cmake --build build --target aphelion
```

## How to runs

Once compiled succesfully, your optimized executable file will be stored directy inside the generated build folder. Execute it from your terminal:

```bash
./build/aphelion
```

## Cleaning the Project

If you ever need to perform a completely fresh re-compilation or wipe the build state, simply delete the generated build output tracking folder:

```bash
rm -r build/
```
