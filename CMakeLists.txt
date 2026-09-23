cmake_minimum_required(VERSION 3.20)
project(LingoLens VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core/include)

# Subdirectories
add_subdirectory(core)
