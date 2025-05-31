cmake_minimum_required(VERSION 3.15)
project(ynetserver
VERSION 0.1.0
DESCRIPTION "Yarnnet server for online multiplayer games with rooms"
LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add thirdparty directory to include paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/enet)

# Add source files
set(SOURCES
    thirdparty/enet_compress.cpp
    main.cpp
    ynetserver.cpp
    ynetclient.cpp
    message.cpp
    thirdparty/mongoose.c
)

# Create executable
add_executable(ynetserver ${SOURCES})

# Set compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ynetserver PRIVATE -Wall -Wextra -Wpedantic)
endif()