# PVB

[English](README.md) | [Română](docs/README-RO.md)

PVB is a visual programming language built around a block-based editor. Programs are created by connecting blocks together instead of writing text, making it easy to experiment, prototype, and understand program flow. Projects can be translated into source code, with support for C++ and Python.

Documentation for Infoeducație is [here](docs/PVB_Documentatie_Infoeducatie_2026.pdf)

## Requirements

### Building PVB

To compile PVB itself, you need a C++20-compatible compiler and CMake.

Supported compilers include:
- GCC
- Clang
- MSVC

### Running generated programs

Generated programs require the appropriate tools for their language:

- C++ output requires a C++ compiler if you want to compile it into an executable.
- Python output requires a Python 3 interpreter.

PVB does not include these tools; they must be installed separately on your system.

## Cloning

Clone the repository with its submodules:

```bash
git clone --recursive https://github.com/mcostn/pvb.git
cd pvb
```

If you already cloned the repository without submodules:

```bash
git submodule update --init --recursive
```

## Building

### Windows

Initialize the build directory:

```bat
init.bat
```

Build the project:

```bat
cmake --build build --parallel
```

### Linux

Initialize the build directory:

```bash
./init.sh
```

Build the project:

```bash
cmake --build build --parallel
```

## Configuration

The initialization scripts accept the following options:

| Option | Description |
|--------|-------------|
| `-d`, `--debug` | Configure a Debug build (default). |
| `-r`, `--release` | Configure a Release build. |
| `-c`, `--clean` | Remove the build directory before configuring. |
| `--reconfigure` | Re-run CMake configuration. |
