################################################################################
# Module: CMakeLists.txt
#
################################################################################

cmake_minimum_required(VERSION 3.16)
project (run_in_job)

# Find all cpp files
FILE(GLOB cpp_sources *.cpp)

# Create the executable
add_executable (run_in_job ${cpp_sources})

target_include_directories(run_in_job PRIVATE inc)

if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
else()
    message(FATAL_ERROR "Only supported on Windows.")
endif()