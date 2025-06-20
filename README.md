# PCD Project

A client-server app for file processing.

## TODO

To be added.

## Project setup and build

### Linux-based components (C server, C client, C admin client)

#### 1. GCC

You need a compiler. We are going with [GCC](https://gcc.gnu.org/). Also, we
are going to install a few more tools which we might need.

| Operating System  | Command to Install GCC and Build Essentials  |
|-------------------|----------------------------------------------|
| Debian-based      | `sudo apt install build-essential`      |
| Fedora-based      | `sudo dnf install gcc gcc-c++ make`     |

#### 2. CMake

[CMake](https://cmake.org/)is an open-source, cross-platform build system
generator.

| Operating System  | Command to Install CMake  |
|-------------------|---------------------------|
| Debian-based      | `sudo apt install cmake`  |
| Fedora-based      | `sudo dnf install cmake`  |

#### 3. Installing Libraries

We are going to need some external libraries, but they should be easily installed
with a package manager:

| Operating System  | Command to Install libraries  |
|-------------------|-----------------------------|
| Debian-based      | `sudo apt install libreadline-dev`  |
| Fedora-based      | `sudo dnf install readline-devel`   |

External libraries used this far:
- readline

#### 4. Build, baby, build

Just run the `build.sh` script. 

You can specify a --target option that can build
just all, just one of the applications, or clean to clean the build folder.

Example:

```bash
./build.sh all # Builds all 3 programs, this is default
./build.sh c-admin-client # This builds just the c-admin-client
./build.sh c-client # This builds just the c-client
./build.sh server # This builds just the server
./build.sh clean # Cleans the build directory
```

### Windows-based components (Python client)

#### 1. Python 3

If not installed, download from [python.org](https://www.python.org/downloads/)
and run the installer.

#### 2. Pip 

Pip should be installed by default with Python. If it isn't, run:

```cmd
python -m ensurepip --upgrade
```
#### 3. Build, baby, build

Just run the `build.bat` script. 

## Contribute

Before contributing to this project, please read the docs/ files, especially
[Programming guidelines](docs/programming-guidelines.md) and [Project
structure](docs/project-structure.md).

