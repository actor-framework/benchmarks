cmake_minimum_required(VERSION 2.6)
project(scala_benchmarks)

set (SCALAC_ARGS "-feature")

set (SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set (CLS_PATH "${EXECUTABLE_OUTPUT_PATH}/org/libcppa")

set (Utility_CLS "${CLS_PATH}/utility/Utility.class")

macro(add_scala_file name package_name)
  add_custom_command(OUTPUT "${CLS_PATH}/${package_name}/${name}.class"
                     COMMAND "${SCALAC}" ${SCALAC_ARGS} "${SRC_PATH}/${name}.scala"
                     DEPENDS "${SRC_PATH}/${name}.scala" ${ARGN}
                     WORKING_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
                     VERBATIM)
  set (CLS_FILES ${CLS_PATH}/${package_name}/${name}.class ${CLS_FILES})
endmacro()

add_scala_file("Utility" "utility")
add_scala_file("ActorCreation" "actor_creation" "${Utility_CLS}")
add_scala_file("Distributed" "distributed" "${Utility_CLS}")
add_scala_file("MailboxPerformance" "mailbox_performance" "${Utility_CLS}")
add_scala_file("Matching" "matching" "${Utility_CLS}")
add_scala_file("MixedCase" "mixed_case" "${Utility_CLS}")

add_custom_target(scalabench DEPENDS ${CLS_FILES})
