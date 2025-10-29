cmake_minimum_required(VERSION 3.15)

project(masp C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Libiberty REQUIRED)

add_subdirectory(src)


