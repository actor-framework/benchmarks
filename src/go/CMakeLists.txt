cmake_minimum_required(VERSION 2.6)
project(go_benchmarks)

set (SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set (OUT_PATH "${EXECUTABLE_OUTPUT_PATH}")

macro(build_package name)
  add_custom_command(OUTPUT "${OUT_PATH}/go/${name}"
                     COMMAND "mkdir" "-p" "${OUT_PATH}/go/"
                     COMMAND "go" "build" "-o" "${OUT_PATH}/go/${name}"
                     DEPENDS "${SRC_PATH}/${name}/*.go"
                     WORKING_DIRECTORY "${SRC_PATH}/${name}"
                     VERBATIM)
  set (OUT_FILES "${OUT_PATH}/go/${name}" ${OUT_FILES})
endmacro()

build_package("mixed_case")

add_custom_target(gobench DEPENDS ${OUT_FILES})
