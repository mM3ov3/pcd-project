# CMake minimum version requirement
cmake_minimum_required(VERSION 3.10)

# Project name
project(pcd-project)

# Set the C standard to C99 (or any other version you need)
set(CMAKE_C_STANDARD 99)


# ========================================
# FIND SOURCE FILES
# ========================================

# Specify the source directories
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# Find all common C source files 
file(GLOB COMMON_SOURCES
    ${SRC_DIR}/common/*.c
)

# Specify component-specific source files
file(GLOB SERVER_SOURCES
    ${SRC_DIR}/server/*.c
)

file(GLOB C_CLIENT_SOURCES
    ${SRC_DIR}/c-client/*.c
)

file(GLOB C_ADMIN_CLIENT_SOURCES
    ${SRC_DIR}/c-admin-client/*.c
)

# ========================================
# FIND AND LINK EXTERNAL LIBS (STATIC)
# ========================================

# Find the libuuid library (installed system-wide)
find_library(UUID_LIB NAMES uuid)


# ========================================
# BUILD COMMON LIB (STATIC)
# ========================================

# Common library target (includes all common code)
add_library(common STATIC ${COMMON_SOURCES})
# Specify the public include directories for common
target_include_directories(common PUBLIC ${SRC_DIR}/common)
# Link external libs here
target_link_libraries(common PUBLIC ${UUID_LIB})


# ========================================
# BUILD TARGETS AND LINKING
# ========================================

# SERVER TARGET AND LINKING
# Define server executable
add_executable(server ${SERVER_SOURCES})
# Link server to necessary libs
target_link_libraries(server 
	common)

# C-ADMIN-CLIENT TARGET AND LINKING
# Define c-admin-client executable
add_executable(c-admin-client ${C_ADMIN_CLIENT_SOURCES})
# Link c-admin-client to the necessary libs
target_link_libraries(c-admin-client 
	common)

# C-CLIENT TARGET AND LINKING
# Define c-client executable
add_executable(c-client ${C_CLIENT_SOURCES})
# Link c-client to the necessary libs
target_link_libraries(c-client 
	common)
