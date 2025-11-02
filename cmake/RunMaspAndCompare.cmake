# Variables expected:
#  masp_exe - path to masp executable
#  src      - input source file
#  expected - expected output file
#  out      - path to write actual output

if(NOT EXISTS "${masp_exe}")
  message(FATAL_ERROR "masp executable not found: ${masp_exe}")
endif()
if(NOT EXISTS "${src}")
  message(FATAL_ERROR "input source not found: ${src}")
endif()
if(NOT EXISTS "${expected}")
  message(FATAL_ERROR "expected file not found: ${expected}")
endif()

get_filename_component(out_dir "${out}" DIRECTORY)
file(MAKE_DIRECTORY "${out_dir}")

execute_process(
  COMMAND "${masp_exe}" -p -s -c ";" -o "${out}" "${src}"
  RESULT_VARIABLE run_rc
  OUTPUT_VARIABLE run_out
  ERROR_VARIABLE run_err
)
if(NOT run_rc EQUAL 0)
  message(FATAL_ERROR "masp failed (rc=${run_rc})\n${run_out}\n${run_err}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files "${expected}" "${out}"
  RESULT_VARIABLE cmp_rc
)
if(NOT cmp_rc EQUAL 0)
  message(FATAL_ERROR "Output differs from expected: ${out}\nExpected: ${expected}")
endif()

message(STATUS "Test passed for ${src}")


