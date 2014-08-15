cmake_minimum_required(VERSION 2.8.6)
project(benchmarks)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(all_benchmarks ALL)

################################################################################
#                                   Utility                                    #
################################################################################

macro(add_xxx_benchmark dir name src_ending bin_ending out_path)
  string(TOUPPER "${dir}" benchname)
  set(OUT_FILE ${out_path}/${name}.${bin_ending})
  set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${name}.${src_ending})
  set(CAF_${benchname}_SRC_FILES ${SRC_FILE} ${CAF_${benchname}_SRC_FILES})
  set(CAF_${benchname}_${name}_OUT ${OUT_FILE})
  set(CAF_${benchname}_OUT_FILES ${OUT_FILE} ${CAF_${benchname}_OUT_FILES})
  add_custom_command(OUTPUT ${OUT_FILE}
                     COMMAND ${CAF_${benchname}_COMPILER} ${SRC_FILE}
                     DEPENDS ${SRC_FILE} ${ARGN}
                     WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
                     VERBATIM)
endmacro()


################################################################################
#                                    Scala                                     #
################################################################################

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
  message(STATUS "Add Scala benchmarks to build (found version \"${ScalacVersion}\")")
  set(CLASS_PATH ${EXECUTABLE_OUTPUT_PATH}/org/caf)
  macro(add_scala_benchmark name)
    add_xxx_benchmark(scala ${name} scala class ${CLASS_PATH} ${ARGN})
  endmacro()
  add_scala_benchmark(utility)
  add_scala_benchmark(actor_creation ${CAF_SCALA_utility_OUT})
  add_scala_benchmark(mailbox_performance ${CAF_SCALA_utility_OUT})
  add_scala_benchmark(mixed_case ${CAF_SCALA_utility_OUT})
  add_custom_target(scala_benchmarks
                    SOURCES ${CAF_SCALA_SRC_FILES}
                    DEPENDS ${CAF_SCALA_OUT_FILES})
  add_dependencies(all_benchmarks scala_benchmarks)
endif()


################################################################################
#                                    Erlang                                    #
################################################################################

# search for Erlang compiler if not disabled by the user
if(NOT CAF_NO_ERLANG_BENCHMARKS)
  if(NOT CAF_ERLANG_COMPILER)
    set(CAF_ERLANG_COMPILER "erlc")
  endif()
  execute_process(COMMAND "${CAF_ERLANG_COMPILER}"
                  RESULT_VARIABLE ErlangResult
                  ERROR_VARIABLE ErlangVersionStr)
  if(NOT ErlangResult EQUAL 0)
      message(STATUS "Disable Erlang benchmarks: could not find a running compiler")
    set(CAF_NO_ERLANG_BENCHMARKS yes)
  endif()
endif()
# add target for Erlang benchmarks
if(NOT CAF_NO_ERLANG_BENCHMARKS)
  message(STATUS "Add Erlang benchmarks to build")
  macro(add_erlang_benchmark name)
    add_xxx_benchmark(erlang ${name} erl beam ${EXECUTABLE_OUTPUT_PATH} ${ARGN})
  endmacro()
  add_erlang_benchmark(actor_creation)
  add_erlang_benchmark(mailbox_performance)
  add_erlang_benchmark(mixed_case)
  add_custom_target(erlang_benchmarks
                    SOURCES ${CAF_ERLANG_SRC_FILES}
                    DEPENDS ${CAF_ERLANG_OUT_FILES})
  add_dependencies(all_benchmarks erlang_benchmarks)
endif()


################################################################################
#                                    Charm++                                   #
################################################################################

if(NOT CAF_NO_CHARM_BENCHMARK)
  if(NOT CAF_CHARM_COMPILER)
    set(CAF_CHARM_COMPILER "charmc")
  endif()
  execute_process(COMMAND "${CAF_CHARM_COMPILER}" -v
                  RESULT_VARIABLE CharmResult
                  ERROR_VARIABLE CharmVersionStr)
  if(NOT CharmResult EQUAL 0)
      message(STATUS "Disable Charm++ benchmarks: could not find a running compiler")
    set(CAF_NO_CHARM_BENCHMARK yes)
  endif()
endif()
# add target for Charm++ benchmarks
if(NOT CAF_NO_ERLANG_BENCHMARKS)
  message(STATUS "Add Charm++ benchmarks to build")
  set(CAF_CHARM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/charm")
  macro(add_charm_benchmark name)
    set(longname charm_${name})
    set(longpath ${CAF_CHARM_PATH}/${longname})
    set(CAF_CHARM_SRC_FILES ${longpath}.ci ${longpath}.cpp ${CAF_CHARM_SRC_FILES})
    add_custom_command(OUTPUT ${longpath}.def.h
                       COMMAND ${CAF_CHARM_COMPILER} ${longpath}.ci
                       DEPENDS ${longpath}.ci
                       WORKING_DIRECTORY ${CAF_CHARM_PATH}
                       VERBATIM)
    add_custom_command(OUTPUT ${longpath}.o
                       COMMAND ${CAF_CHARM_COMPILER} -c ${longpath}.cpp
                       DEPENDS ${longpath}.cpp ${longpath}.def.h
                       WORKING_DIRECTORY ${CAF_CHARM_PATH}
                       VERBATIM)
    set(exe_file ${EXECUTABLE_OUTPUT_PATH}/${longname})
    set(CAF_CHARM_OUT_FILES ${exe_file} ${CAF_CHARM_OUT_FILES})
    add_custom_command(OUTPUT ${exe_file}
                       COMMAND ${CAF_CHARM_COMPILER} ${longpath}.o -o ${exe_file} -language charm++
                       DEPENDS ${longpath}.o
                       WORKING_DIRECTORY ${CAF_CHARM_PATH}
                       VERBATIM)
  endmacro()
  add_charm_benchmark(actor_creation)
  add_charm_benchmark(mailbox_performance)
  add_charm_benchmark(mixed_case)
  add_custom_target(charm_benchmarks
                    SOURCES ${CAF_CHARM_SRC_FILES}
                    DEPENDS ${CAF_CHARM_OUT_FILES})
  add_dependencies(all_benchmarks charm_benchmarks)
else()
  # make files show up in GUI even if not building Charm++
  file(GLOB_RECURSE CHARM_SOURCES
       "${CMAKE_CURRENT_SOURCE_DIR}/charm++/*.cpp"
       "${CMAKE_CURRENT_SOURCE_DIR}/charm++/*.ci")
  add_custom_target(caf_charm_bench_dummy SOURCES ${MISC_FILES})
endif()



################################################################################
#                                     CAF                                      #
################################################################################

# add targets for CAF benchmarks
macro(add_caf_benchmark name)
  add_executable(${name} ${CMAKE_CURRENT_SOURCE_DIR}/caf/${name}.cpp)
  target_link_libraries(${name} ${LIBCAF_LIBRARIES} ${LD_FLAGS})
  add_dependencies(all_benchmarks ${name})
endmacro()

add_caf_benchmark(actor_creation)
add_caf_benchmark(mailbox_performance)
add_caf_benchmark(mixed_case)


################################################################################
#                                    tools                                     #
################################################################################

find_package(Boost)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  add_executable(to_dat ${CMAKE_CURRENT_SOURCE_DIR}/tools/to_dat.cpp)
  target_link_libraries(to_dat ${LD_FLAGS})
endif()
add_executable(caf_run_bench ${CMAKE_CURRENT_SOURCE_DIR}/caf_run_bench.cpp)
target_link_libraries(caf_run_bench ${LIBCAF_LIBRARIES} ${LD_FLAGS})
add_dependencies(all_benchmarks caf_run_bench)
add_custom_target(caf_scripts_dummy SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/run)
