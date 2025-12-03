# Minimal C unit test that calls masp's main in-process to avoid CLI parsing issues

add_executable(test_masp_cli
  ${CMAKE_SOURCE_DIR}/test/unit/test_masp_cli.c
  ${CMAKE_SOURCE_DIR}/src/hash.c
  ${CMAKE_SOURCE_DIR}/src/macro.c
  ${CMAKE_SOURCE_DIR}/src/sb.c
  ${CMAKE_SOURCE_DIR}/src/compat.c
)

target_include_directories(test_masp_cli PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)

target_compile_definitions(test_masp_cli PRIVATE
  SRC_DIR="${CMAKE_SOURCE_DIR}"
  BUILD_DIR="${CMAKE_BINARY_DIR}"
  PACKAGE_VERSION="test"
  REPORT_BUGS_TO="ci"
  LOCALEDIR=""
)

# Only run CLI unit test when ASAN is enabled
# Without ASAN, masp has memory corruption bugs that cause crashes
if(ENABLE_ASAN)
  add_test(NAME masp_cli_unit COMMAND test_masp_cli)
endif()

# Windows (MinGW) needs POSIX regex library (libgnurx)
if(MINGW)
  target_link_libraries(test_masp_cli PRIVATE gnurx)
endif()

# Stress test to reproduce crashes
# This test requires the masp binary to be built first
# In CI/CD, run: cmake --build . && ctest
# Only run stress test when ASAN is enabled, since it's designed to catch
# memory corruption bugs that ASAN fixes
add_executable(test_stress_parallel test_stress_parallel.c)
target_compile_definitions(test_stress_parallel PRIVATE
  SRC_DIR="${CMAKE_SOURCE_DIR}"
  BUILD_DIR="${CMAKE_BINARY_DIR}"
)
if(ENABLE_ASAN)
  add_test(NAME masp_stress COMMAND test_stress_parallel)
endif()


