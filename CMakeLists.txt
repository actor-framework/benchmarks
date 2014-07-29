cmake_minimum_required (VERSION 2.8.6)
project (benchmarks)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(all_benchmarks)

# search for Scala compiler if not disabled by the user
if(NOT CAF_NO_SCALA_BENCHMARKS)
  if(NOT CAF_SCALA_COMPILER)
    set(CAF_SCALA_COMPILER "scalac")
  endif()
  execute_process(COMMAND "${CAF_SCALA_COMPILER}" "-version"
                  RESULT_VARIABLE ScalacResult
                  ERROR_VARIABLE ScalacVersionStr)
  if(ScalacResult EQUAL 0)
    string(REGEX REPLACE ".*version ([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1"
           ScalacVersion "${ScalacVersionStr}")
    if(ScalacVersion VERSION_LESS 2.10)
      message(STATUS "Disable Scala benchmarks: version >= 2.10 required, ${ScalacVersion} found")
      set(CAF_NO_SCALA_BENCHMARKS yes)
    endif()
  else()
      message(STATUS "Disable Scala benchmarks: could not find a running compiler")
    set(CAF_NO_SCALA_BENCHMARKS yes)
  endif()
endif()

# add targets for Scala benchmarks
if(NOT CAF_NO_SCALA_BENCHMARKS)
  set(CAF_SCALABENCH_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/scala)
  set(CAF_SCALABENCH_BIN_PATH ${EXECUTABLE_OUTPUT_PATH}/org/caf)
  macro(add_scala_target name)
    add_custom_command(OUTPUT ${CAF_SCALABENCH_BIN_PATH}/${name}.class
                       COMMAND ${CAF_SCALA_COMPILER}
                               ${CAF_SCALABENCH_SRC_PATH}/${name}.scala
                       DEPENDS ${CAF_SCALABENCH_SRC_PATH}/${name}.scala
                               ${ARGN}
                       WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
                       VERBATIM)
    set(CAF_SCALABENCH_${name} ${CAF_SCALABENCH_BIN_PATH}/${name}.class)
    set(CAF_SCALABENCH_CLASS_FILES
        ${CAF_SCALABENCH_${name}}
        ${CAF_SCALABENCH_CLASS_FILES})
  endmacro()
  add_scala_target(utility)
  add_scala_target(actor_creation ${CAF_SCALABENCH_utility})
  add_scala_target(mailbox_performance ${CAF_SCALABENCH_utility})
  add_scala_target(mixed_case ${CAF_SCALABENCH_utility})
  add_custom_target(scala_benchmarks DEPENDS ${CAF_SCALABENCH_CLASS_FILES})
  add_dependencies(scala_benchmarks all_benchmarks)
endif()

# add targets for CAF benchmarks
macro(add_caf_benchmark name)
  add_executable(${name} ${CMAKE_CURRENT_SOURCE_DIR}/caf/${name}.cpp)
  target_link_libraries(${name} ${LIBCAF_LIBRARIES} ${LD_FLAGS})
  add_dependencies(${name} all_benchmarks)
endmacro()

add_caf_benchmark(actor_creation)
add_caf_benchmark(mailbox_performance)
add_caf_benchmark(mixed_case)
#add_caf_benchmark(distributed)
#add_caf_benchmark(matching)
#add_caf_benchmark(matrix)
