# Aphelion

The identity of this project is still being defined. As of now, the idea is to make a free, open-source AI coding agent that runs in your terminal.

## Prerequisites
Before building this project, ensure you have the following utilities installed on your system:

* **CMake** (Version 3.15 or higher)
* **C++ Compiler** (GCC, Clang, or MSVC supporting C++17)
* **Build Tool** (Make or Ninja)
* **libcurl** (development headers, used by the AI provider layer to talk to APIs)

## Project Structure

```txt
aphelion/
├── CMakeLists.txt                       # Core CMake configuration script
├── README.md                            # Project documentation and setup guide
├── themes                               # Default theme JSON - reference copies only, see below
├── thinkers                             # Default thinker JSON - reference copies only, see below
└── src                                  # Aphelion source files
    ├── main.cpp
    ├── config_paths.h/.cpp               # Resolves ~/.aphelion/ - see Configuration below
    ├── default_assets.h                  # Default theme/thinker JSON embedded into the binary
    ├── ai                                # AI provider abstraction layer (Gemini, OpenRouter, Ollama)
    └── tools                             # Tools the AI can call (scan_project, etc.)
```

## Configuration & Data Directory

**Breaking change (pre-alpha):** Aphelion no longer reads `settings.json`,
`history/`, `themes/`, or `thinkers/` from the current working directory.
All of that now lives under `~/.aphelion/` (`$HOME` on Linux/macOS,
`%USERPROFILE%` on Windows), resolved the same way regardless of which
directory you run the binary from:

```txt
~/.aphelion/
├── settings.json    # created with defaults on first run
├── history/         # saved sessions and /scan digests
├── themes/          # seeded from the built-in defaults on first run
└── thinkers/        # seeded from the built-in defaults on first run
```

This directory is created automatically the first time you run Aphelion -
nothing to set up by hand. The `themes/`/`thinkers/` folders in the repo are
kept as the human-readable source the built-in defaults in
`src/default_assets.h` are generated from; they aren't read at runtime.
There's no migration path from a pre-existing repo-root `settings.json` or
`history/` - if you have one from before this change, it's now unused and
safe to delete.

## AI Providers

Aphelion talks to whichever AI provider is set in `settings.json` under `"ai"`. Three are supported:

* **gemini** — Google Gemini via ai.dev
* **openrouter** — OpenRouter
* **ollama** — a local Ollama instance

Each provider has its own `model` and, for Gemini/OpenRouter, an API key. Keys can be set directly as `api_key` in `settings.json`, but it's safer to leave that blank and export the corresponding environment variable instead (`GEMINI_API_KEY` / `OPENROUTER_API_KEY` by default — configurable via `api_key_env`).

Switch provider or model at runtime from the REPL:

```txt
/provider <gemini|openrouter|ollama>
/model <model_name>
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

## How to run

Once compiled succesfully, your optimized executable file will be stored directy inside the generated build folder. Execute it from your terminal:

```bash
./build/aphelion
```

## Cleaning the Project

If you ever need to perform a completely fresh re-compilation or wipe the build state, simply delete the generated build output tracking folder:

```bash
rm -r build/
```
