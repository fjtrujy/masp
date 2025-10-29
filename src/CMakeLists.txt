# Sources for MASP
set(MASP_SOURCES
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
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(masp PRIVATE PACKAGE_VERSION="0.1.16")

# Link external libiberty (required by top-level)
target_link_libraries(masp PRIVATE Libiberty::Libiberty)
