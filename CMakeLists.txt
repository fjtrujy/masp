cmake_minimum_required(VERSION 3.15)

project(masp C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# AddressSanitizer support (disabled by default)
# Enable with -DENABLE_ASAN=ON when needed
option(ENABLE_ASAN "Enable AddressSanitizer for memory safety" OFF)
if(ENABLE_ASAN)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

# Enable CTest
include(CTest)
enable_testing()

add_subdirectory(src)

if(BUILD_TESTING)
  add_subdirectory(test)
  # Ensure invoking the test target compiles the app first
  if(TARGET test)
    add_dependencies(test masp)
  endif()
  # Provide a convenient 'check' target that builds and runs tests
  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure
    DEPENDS masp
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
endif()

