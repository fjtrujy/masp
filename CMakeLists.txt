cmake_minimum_required(VERSION 3.15)

project(masp C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable CTest
include(CTest)
enable_testing()

add_subdirectory(src)


