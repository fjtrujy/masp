# Sources for MASP
set(MASP_SOURCES
  compat.c
  masp.c
  macro.c
  sb.c
  hash.c
)

# Generate a minimal config.h for CMake builds
configure_file(${CMAKE_SOURCE_DIR}/cmake/app_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)

add_executable(masp ${MASP_SOURCES})

target_include_directories(masp
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(masp PRIVATE PACKAGE_VERSION="0.1.16")
