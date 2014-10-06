cmake_minimum_required(VERSION 2.8.6)
project(benchmarks)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(all_benchmarks ALL)

################################################################################
#                                   Utility                                    #
################################################################################

macro(add_xxx_benchmark benchname dir name src_ending bin_ending out_path)
  set(OUT_FILE ${out_path}/${name}.${bin_ending})
  set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${name}.${src_ending})
  set(CAF_${benchname}_SRC_FILES ${SRC_FILE} ${CAF_${benchname}_SRC_FILES})
  set(CAF_${benchname}_${name}_OUT ${OUT_FILE})
  set(CAF_${benchname}_OUT_FILES ${OUT_FILE} ${CAF_${benchname}_OUT_FILES})
  add_custom_command(OUTPUT ${OUT_FILE}
                     COMMAND ${CAF_${benchname}_COMPILER} ${CAF_${benchname}_COMPILER_OPTS} ${SRC_FILE}
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
  set(CLASS_PATH ${EXECUTABLE_OUTPUT_PATH}/org/caf/scala)
  set(CAF_SCALA_COMPILER_OPTS -optimize)
  macro(add_scala_benchmark name)
    add_xxx_benchmark(SCALA scala ${name} scala class ${CLASS_PATH} ${ARGN})
  endmacro()
  add_scala_benchmark(utility)
  add_scala_benchmark(actor_creation ${CAF_SCALA_utility_OUT})
  add_scala_benchmark(mailbox_performance ${CAF_SCALA_utility_OUT})
  add_scala_benchmark(mixed_case ${CAF_SCALA_utility_OUT})
  add_scala_benchmark(mandelbrot ${CAF_SCALA_utility_OUT})
  add_custom_target(scala_benchmarks
                    SOURCES ${CAF_SCALA_SRC_FILES}
                    DEPENDS ${CAF_SCALA_OUT_FILES})
  add_dependencies(all_benchmarks scala_benchmarks)
endif()


################################################################################
#                                    SALSA                                     #
################################################################################

if(NOT CAF_NO_SALSA_BENCHMARKS)
  if(NOT CAF_SALSA_JAR)
    # try find it in home directory
    set(CAF_SALSA_JAR "$ENV{HOME}/salsa_lite2.0.3.jar")
  endif()
  if(NOT EXISTS "${CAF_SALSA_JAR}")
    message(STATUS "Disable SALAS benchmarks: salsa JAR file not found")
    set(CAF_NO_SALSA_BENCHMARKS yes)
  else()
    if(NOT CAF_JAVA_BIN)
      set(CAF_JAVA_BIN "java")
    endif()
    execute_process(COMMAND "${CAF_JAVA_BIN}" "-version"
                    RESULT_VARIABLE JavaResult
                    ERROR_VARIABLE JavaVersionStr)
    if(JavaResult EQUAL 0)
      string(REGEX REPLACE ".*version \"([0-9]+\\.[0-9]+\\.[0-9_]+)\".*" "\\1"
             JavaVersion "${JavaVersionStr}")
    else()
      message(STATUS "Disable SALSA benchmarks: unable to run java")
      set(CAF_NO_SALSA_BENCHMARKS yes)
    endif()
    if(NOT CAF_JAVA_COMPILER)
      set(CAF_JAVA_COMPILER "javac")
    endif()
    execute_process(COMMAND "${CAF_JAVA_COMPILER}" "-version"
                    RESULT_VARIABLE JavacResult
                    ERROR_VARIABLE JavacVersionStr)
    if(JavacResult EQUAL 0)
      string(REGEX REPLACE ".*version \"([0-9]+\\.[0-9]+\\.[0-9_]+)\".*" "\\1"
             JavacVersion "${JavacVersionStr}")
    else()
      message(STATUS "Disable SALSA benchmarks: unable to run java")
      set(CAF_NO_SALSA_BENCHMARKS yes)
    endif()
  endif()
endif()
# add targets for SALSA benchmarks
if(NOT CAF_NO_SALSA_BENCHMARKS)
  set(CLASS_PATH ${EXECUTABLE_OUTPUT_PATH}/org/caf/salsa)
  set(JAVAC_OPTS -cp "${CAF_SALSA_JAR}:." -d ${EXECUTABLE_OUTPUT_PATH})
  set(SALSA_OPTS -cp "${CAF_SALSA_JAR}:." salsa_lite.core.compiler.SalsaCompiler)
  message(STATUS "Add SALSA benchmarks to build (found java \"${JavaVersion}\")")
  set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/salsa)
  macro(add_salsa_benchmark salsa_files java_files class_files)
    add_custom_command(OUTPUT ${java_files}
                       COMMAND ${CAF_JAVA_BIN} ${SALSA_OPTS} ${salsa_files}
                       DEPENDS ${salsa_files}
                       WORKING_DIRECTORY ${SRC_DIR}
                       VERBATIM)
    add_custom_command(OUTPUT ${class_files}
                       COMMAND ${CAF_JAVA_COMPILER} ${JAVAC_OPTS} ${java_files}
                       DEPENDS ${java_files}
                       WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
                       VERBATIM)
  endmacro()
  # actor creation
  set(ACTOR_CREATION_SALSA_FILES ${SRC_DIR}/actor_creation.salsa)
  set(ACTOR_CREATION_JAVA_FILES ${SRC_DIR}/actor_creation.java
                                ${SRC_DIR}/actor_creationState.java)
  set(ACTOR_CREATION_CLASS_FILES ${CLASS_PATH}/actor_creation.class
                                 ${CLASS_PATH}/actor_creationState.class)
  add_salsa_benchmark("${ACTOR_CREATION_SALSA_FILES}"
                      "${ACTOR_CREATION_JAVA_FILES}"
                      "${ACTOR_CREATION_CLASS_FILES}")
  # mixed case
  set(MIXED_CASE_SALSA_FILES ${SRC_DIR}/chain_link.salsa
                             ${SRC_DIR}/chain_master.salsa
                             ${SRC_DIR}/supervisor.salsa
                             ${SRC_DIR}/worker.salsa
                             ${SRC_DIR}/mixed_case.salsa)
  set(MIXED_CASE_JAVA_FILES ${SRC_DIR}/chain_link.java
                            ${SRC_DIR}/chain_linkState.java
                            ${SRC_DIR}/chain_master.java
                            ${SRC_DIR}/chain_masterState.java
                            ${SRC_DIR}/supervisor.java
                            ${SRC_DIR}/supervisorState.java
                            ${SRC_DIR}/worker.java
                            ${SRC_DIR}/workerState.java
                            ${SRC_DIR}/mixed_case.java
                            ${SRC_DIR}/mixed_caseState.java)
  set(MIXED_CASE_CLASS_FILES ${CLASS_PATH}/chain_link.class
                             ${CLASS_PATH}/chain_master.class
                             ${CLASS_PATH}/supervisor.class
                             ${CLASS_PATH}/worker.class
                             ${CLASS_PATH}/mixed_case.class)
  add_salsa_benchmark("${MIXED_CASE_SALSA_FILES}"
                      "${MIXED_CASE_JAVA_FILES}"
                      "${MIXED_CASE_CLASS_FILES}")
  # mailbox performance
  set(MAILBOX_PERFORMANCE_SALSA_FILES
      ${SRC_DIR}/sender.salsa
      ${SRC_DIR}/receiver.salsa
      ${SRC_DIR}/mailbox_performance.salsa)
  set(MAILBOX_PERFORMANCE_JAVA_FILES
      ${SRC_DIR}/sender.java
      ${SRC_DIR}/senderState.java
      ${SRC_DIR}/receiver.java
      ${SRC_DIR}/receiverState.java
      ${SRC_DIR}/mailbox_performance.java
      ${SRC_DIR}/mailbox_performanceState.java)
  set(MAILBOX_PERFORMANCE_CLASS_FILES
      ${CLASS_PATH}/sender.class
      ${CLASS_PATH}/senderState.class
      ${CLASS_PATH}/receiver.class
      ${CLASS_PATH}/receiverState.class
      ${CLASS_PATH}/mailbox_performance.class
      ${CLASS_PATH}/mailbox_performanceState.class)
  add_salsa_benchmark("${MAILBOX_PERFORMANCE_SALSA_FILES}"
                      "${MAILBOX_PERFORMANCE_JAVA_FILES}"
                      "${MAILBOX_PERFORMANCE_CLASS_FILES}")
  file(GLOB_RECURSE SALSA_SOURCES "${SRC_DIR}/*.salsa")
  add_custom_target(salsa_benchmarks
                    SOURCES ${SALSA_SOURCES}
                    DEPENDS ${ACTOR_CREATION_CLASS_FILES}
                            ${MIXED_CASE_CLASS_FILES}
                            ${MAILBOX_PERFORMANCE_CLASS_FILES})
  add_dependencies(all_benchmarks salsa_benchmarks)
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
  set(CAF_ERLANG_COMPILER_OPTS "+native" "+{hipe, [o3]}")
  macro(add_erlang_benchmark name)
    add_xxx_benchmark(ERLANG erlang ${name} erl beam ${EXECUTABLE_OUTPUT_PATH} ${ARGN})
  endmacro()
  add_erlang_benchmark(actor_creation)
  add_erlang_benchmark(mailbox_performance)
  add_erlang_benchmark(mixed_case)
  add_erlang_benchmark(mandelbrot)
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
if(NOT CAF_NO_CHARM_BENCHMARK)
  message(STATUS "Add Charm++ benchmarks to build")
  set(CAF_CHARM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/charm")
  set(CAF_CHARM_COMPILER_OPTS -optimize)
  macro(add_charm_benchmark name)
    set(longname charm_${name})
    set(longpath ${CAF_CHARM_PATH}/${longname})
    set(CAF_CHARM_SRC_FILES ${longpath}.ci ${longpath}.cpp ${CAF_CHARM_SRC_FILES})
    add_custom_command(OUTPUT ${longpath}.def.h ${longpath}.decl.h
                       COMMAND ${CAF_CHARM_COMPILER} ${longpath}.ci
                       DEPENDS ${longpath}.ci
                       WORKING_DIRECTORY ${CAF_CHARM_PATH}
                       VERBATIM)
    add_custom_command(OUTPUT ${longpath}.o
                       COMMAND ${CAF_CHARM_COMPILER} -c ${longpath}.cpp
                       DEPENDS ${longpath}.cpp ${longpath}.def.h ${longpath}.decl.h
                       WORKING_DIRECTORY ${CAF_CHARM_PATH}
                       VERBATIM)
    set(exe_file ${EXECUTABLE_OUTPUT_PATH}/${longname})
    set(CAF_CHARM_OUT_FILES ${exe_file} ${CAF_CHARM_OUT_FILES})
    add_custom_command(OUTPUT ${exe_file}
                       COMMAND ${CAF_CHARM_COMPILER} ${longpath}.o -o ${exe_file} -O3 -language charm++
                       DEPENDS ${longpath}.o
                       WORKING_DIRECTORY ${CAF_CHARM_PATH}
                       VERBATIM)
  endmacro()
  add_charm_benchmark(actor_creation)
  add_charm_benchmark(mailbox_performance)
  add_charm_benchmark(mixed_case)
  add_charm_benchmark(mandelbrot)
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
add_caf_benchmark(mandelbrot)


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
