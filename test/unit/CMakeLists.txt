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

# CLI unit test
add_test(NAME masp_cli_unit COMMAND test_masp_cli)

# Per-module unit tests — link only the module under test, no masp.c
# pull-in.  Keeps the build closed-form and the tests fast.
add_executable(test_sb
  ${CMAKE_SOURCE_DIR}/test/unit/test_sb.c
  ${CMAKE_SOURCE_DIR}/src/sb.c
)
target_include_directories(test_sb PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
add_test(NAME masp_sb_unit COMMAND test_sb)

add_executable(test_hash
  ${CMAKE_SOURCE_DIR}/test/unit/test_hash.c
  ${CMAKE_SOURCE_DIR}/src/hash.c
  ${CMAKE_SOURCE_DIR}/src/compat.c
)
target_include_directories(test_hash PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
add_test(NAME masp_hash_unit COMMAND test_hash)

# Number-prefix parser tests.  Reaches into masp.c statics via direct
# #include (same trick as test_masp_cli); the support modules are
# linked here so the binary is closed-form.
add_executable(test_number_prefix
  ${CMAKE_SOURCE_DIR}/test/unit/test_number_prefix.c
  ${CMAKE_SOURCE_DIR}/src/hash.c
  ${CMAKE_SOURCE_DIR}/src/macro.c
  ${CMAKE_SOURCE_DIR}/src/sb.c
  ${CMAKE_SOURCE_DIR}/src/compat.c
)
target_include_directories(test_number_prefix PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
target_compile_definitions(test_number_prefix PRIVATE
  SRC_DIR="${CMAKE_SOURCE_DIR}"
  BUILD_DIR="${CMAKE_BINARY_DIR}"
  PACKAGE_VERSION="test"
  REPORT_BUGS_TO="ci"
  LOCALEDIR=""
)
if(MINGW)
  target_link_libraries(test_number_prefix PRIVATE gnurx)
endif()
add_test(NAME masp_number_prefix_unit COMMAND test_number_prefix)

# Windows (MinGW) needs POSIX regex library (libgnurx)
if(MINGW)
  target_link_libraries(test_masp_cli PRIVATE gnurx)
endif()

# Stress test for concurrent/repeated masp execution
# This test requires the masp binary to be built first
# In CI/CD, run: cmake --build . && ctest
# Note: This test uses POSIX fork/exec APIs and is not supported on Windows
if(NOT WIN32)
  add_executable(test_stress_parallel test_stress_parallel.c)
  target_compile_definitions(test_stress_parallel PRIVATE
    SRC_DIR="${CMAKE_SOURCE_DIR}"
    BUILD_DIR="${CMAKE_BINARY_DIR}"
  )
  add_test(NAME masp_stress COMMAND test_stress_parallel)
endif()


