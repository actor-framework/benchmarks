cmake_minimum_required(VERSION 2.6)
project(erlang_benchmarks)

set (ERLC "erlc")
set (ERLC_ARGS "")

set (SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set (BEAM_PATH "${EXECUTABLE_OUTPUT_PATH}")

set (Utility_CLS "${BEAM_PATH}/utility/Utility.class")

macro(add_erl_file name)
  add_custom_command(OUTPUT "${BEAM_PATH}/${name}.beam"
                     COMMAND "${ERLC}" ${ERLC_ARGS} "${SRC_PATH}/${name}.erl"
                     DEPENDS "${SRC_PATH}/${name}.erl" ${ARGN}
                     WORKING_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
                     VERBATIM)
  set (BEAM_FILES ${BEAM_PATH}/${name}.beam ${BEAM_FILES})
endmacro()

add_erl_file("actor_creation")
add_erl_file("distributed")
add_erl_file("mailbox_performance")
add_erl_file("matching")
add_erl_file("mixed_case")

add_custom_target(erlangbench DEPENDS ${BEAM_FILES})
