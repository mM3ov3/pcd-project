# Project structure

Naming is hard. Let's start here:

**WHEN YOU CREATE A FILE OR DIRECTORY, PLEASE KEEP THE NAME LOWERCASE (IF
POSSIBLE), SPLIT WORDS INSIDE THE NAME BY - (dash) AND AVOID NUMBERS (IF
POSSIBLE). PLEASE, USE MEANINGFUL NAMES AND NO EMPTY SPACES, MIXING UPPER AND
LOWER CASE LETTERS OR SYMBOLS.**

Choose `file-name.c` over `FiLe naME2^#_.c`.

Here is the initial project structure:
```bash
./
├── build/
│   └── .gitkeep
├── docs/
│   ├── programming-guidelines.md
│   └── project-structure.md
├── src/
│   ├── c-admin-client/
│   │   └── c-admin-client.c
│   ├── c-client/
│   │   └── client.c
│   ├── common/
│   │   ├── common.c
│   │   └── common.h
│   ├── py-client/
│   │   └── client.py
│   └── server/
│       ├── rest/
│       └── server.c
├── build.bat
├── build.sh
├── CMakeLists.txt
├── .gitignore
├── README.md
└── requirements.txt
```
A short overview of the project structure and general guidelines.

- **build/** - Build directory. Executables, libraries and a lot of files
  generated during build are here.
- **docs/** - Documentation directory. Here is the documentation in markdown
  format.
- **src/** - Source directory. Here we should have directory of certain
  components of the app. The `tree` view above should serve as example for how
  files would be organized inside it. **Header files and source files should
  share the same directory for their respective component. DO NOT create
  separate directories for header files (like include/).**
- **src/c-admin-client/** - Here is the implementation of the administrator client
  in C.
- **src/c-client/** - Here is the implementation of the standard client in C.
- **common/** - Whatever common code the clients and server share. Also,
  auxiliary functions (utils). More of a misc directory. THIS WILL BE BUILT AS
  A STATIC LIBRARY AND LINKED ACCORDING TO CMakeLists.txt
- **src/py-client/** - Here is the implementation of the Windows client in Python.
- **src/server/** - Here is the implementation of the server in C.
- **src/server/rest** - If we ever get around making a REST API for our server.
  Empty directory for now.
- **build.bat** - build script for Windows.
- **build.sh** - build script for Linux.
- **CMakeLists.txt** - CMakeLists file that specifies various compiling,
  linking options and also different build targets. This will be hard to manage
  as we keep adding to our project.
- **.gitignore** - File for git to not track files we do not need. Hopefully,
  we track only what we need.
- **README.md** - The readme you probably had read by now.
- **requirements.txt** - File for pip to pull dependecies (external libraries).
  For py-client only.
