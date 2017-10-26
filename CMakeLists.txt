cmake_minimum_required(VERSION 2.8.6)
project(benchmarks)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(all_benchmarks ALL)

set(CAF_COMPILED_BENCHES "")

################################################################################
#                                   Utility                                    #
################################################################################

macro(add_xxx_benchmark benchname dir name src_ending bin_ending out_path)
  set(OUT_FILE ${out_path}/${name}.${bin_ending})
  set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/${name}.${src_ending})
  set(CAF_${benchname}_SRC_FILES ${SRC_FILE} ${CAF_${benchname}_SRC_FILES})
  set(CAF_${benchname}_${name}_OUT ${OUT_FILE})
  set(CAF_${benchname}_OUT_FILES ${OUT_FILE} ${CAF_${benchname}_OUT_FILES})
  add_custom_command(OUTPUT ${OUT_FILE}
                     COMMAND ${CAF_${benchname}_COMPILER}
                             ${CAF_${benchname}_COMPILER_OPTS}
                             ${SRC_FILE}
                     DEPENDS ${SRC_FILE} ${ARGN}
                     WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
                     VERBATIM)
endmacro()

# make files show up in GUI even if not building a particular benchmark
macro(add_dummy_target benchname src_ending)
  # check all aditional dependency flags
  set(globstr "${CMAKE_CURRENT_SOURCE_DIR}/src/${benchname}/*.${src_ending}")
  foreach(additional_ending ${ARGN})
    set(globstr ${globstr}
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${benchname}/*.${additional_ending}")
  endforeach()
  file(GLOB_RECURSE source_files ${globstr})
  add_custom_target(caf_${benchname}_dummy SOURCES ${source_files})
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
      message(STATUS "Disable Scala benchmarks: version >= 2.10 required,"
                     "${ScalacVersion} found")
      set(CAF_NO_SCALA_BENCHMARKS yes)
    endif()
  else()
    message(STATUS "Disable Scala benchmarks: no running compiler found")
    set(CAF_NO_SCALA_BENCHMARKS yes)
  endif()
endif()
# add targets for Scala benchmarks
if(NOT CAF_NO_SCALA_BENCHMARKS)
  message(STATUS "Add Scala benchmarks to build (found ${ScalacVersion})")
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
  set(CAF_COMPILED_BENCHES "scala ${CAF_COMPILED_BENCHES}")
else()
  add_dummy_target(scala scala)
endif()


################################################################################
#                                    SALSA                                     #
################################################################################

if(NOT CAF_NO_SALSA_BENCHMARKS)
  if(NOT CAF_SALSA_JAR)
    # try find it in home directory
    set(CAF_SALSA_JAR "$ENV{HOME}/SalsaLite.jar")
  endif()
  if(NOT EXISTS "${CAF_SALSA_JAR}")
    message(STATUS "Disable SALSA benchmarks: salsa JAR file not found")
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
else()
  set(CAF_SALSA_JAR "")
endif()
# add targets for SALSA benchmarks
if(NOT CAF_NO_SALSA_BENCHMARKS)
  set(CLASS_PATH ${EXECUTABLE_OUTPUT_PATH}/org/caf/salsa)
  set(JAVAC_OPTS -cp "${CAF_SALSA_JAR}:." -d ${EXECUTABLE_OUTPUT_PATH})
  set(SALSA_OPTS -cp "${CAF_SALSA_JAR}:." salsa_lite.compiler.SalsaCompiler)
  message(STATUS "Add SALSA benchmarks to build (found ${JavaVersion})")
  set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/salsa)
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
  set(ACTOR_CREATION_JAVA_FILES  ${SRC_DIR}/actor_creation.java)
  set(ACTOR_CREATION_CLASS_FILES ${CLASS_PATH}/actor_creation.class)
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
                            ${SRC_DIR}/chain_master.java
                            ${SRC_DIR}/supervisor.java
                            ${SRC_DIR}/worker.java
                            ${SRC_DIR}/mixed_case.java)
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
      ${SRC_DIR}/receiver.java
      ${SRC_DIR}/mailbox_performance.java)
  set(MAILBOX_PERFORMANCE_CLASS_FILES
      ${CLASS_PATH}/sender.class
      ${CLASS_PATH}/receiver.class
      ${CLASS_PATH}/mailbox_performance.class)
  add_salsa_benchmark("${MAILBOX_PERFORMANCE_SALSA_FILES}"
                      "${MAILBOX_PERFORMANCE_JAVA_FILES}"
                      "${MAILBOX_PERFORMANCE_CLASS_FILES}")
  # mandelbrot
  set(MANDELBROT_SALSA_FILES
      ${SRC_DIR}/mandelbrot.salsa
      ${SRC_DIR}/mandelbrot_worker.salsa)
  set(MANDELBROT_JAVA_FILES
      ${SRC_DIR}/mandelbrot.java
      ${SRC_DIR}/mandelbrot_worker.java)
  set(MANDELBROT_CLASS_FILES
      ${CLASS_PATH}/mandelbrot.class
      ${CLASS_PATH}/mandelbrot_worker.class)
  add_salsa_benchmark("${MANDELBROT_SALSA_FILES}"
                      "${MANDELBROT_JAVA_FILES}"
                      "${MANDELBROT_CLASS_FILES}")
  file(GLOB_RECURSE SALSA_SOURCES "${SRC_DIR}/*.salsa")
  add_custom_target(salsa_benchmarks
                    SOURCES ${SALSA_SOURCES}
                    DEPENDS ${ACTOR_CREATION_CLASS_FILES}
                            ${MIXED_CASE_CLASS_FILES}
                            ${MAILBOX_PERFORMANCE_CLASS_FILES}
                            ${MANDELBROT_CLASS_FILES})
  add_dependencies(all_benchmarks salsa_benchmarks)
  set(CAF_COMPILED_BENCHES "salsa ${CAF_COMPILED_BENCHES}")
else()
  add_dummy_target(salsa salsa)
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
      message(STATUS "Disable Erlang benchmarks: no running compiler found")
    set(CAF_NO_ERLANG_BENCHMARKS yes)
  endif()
endif()
# add target for Erlang benchmarks
if(NOT CAF_NO_ERLANG_BENCHMARKS)
  message(STATUS "Add Erlang benchmarks to build")
  set(CAF_ERLANG_COMPILER_OPTS "+native" "+{hipe, [o3]}")
  macro(add_erlang_benchmark name)
    add_xxx_benchmark(ERLANG erlang ${name} erl beam
                      "${EXECUTABLE_OUTPUT_PATH}" ${ARGN})
  endmacro()
  macro(add_erlang_bencherl_benchmark name)
    add_xxx_benchmark(ERLANG erlang/bencherl ${name} erl beam
                      "${EXECUTABLE_OUTPUT_PATH}" ${ARGN})
  endmacro()
  add_erlang_benchmark(actor_creation)
  add_erlang_benchmark(mailbox_performance)
  add_erlang_benchmark(mixed_case)
  add_erlang_benchmark(mandelbrot)
  add_erlang_bencherl_benchmark(bencherl_01_bang)
  add_erlang_bencherl_benchmark(bencherl_02_big)
  add_erlang_bencherl_benchmark(bencherl_03_ehb)
  add_erlang_bencherl_benchmark(bencherl_05_genstress)
  add_erlang_bencherl_benchmark(bencherl_06_mbrot)
  add_erlang_bencherl_benchmark(bencherl_08_parallel)
  add_erlang_bencherl_benchmark(bencherl_10_ran)
  add_erlang_bencherl_benchmark(bencherl_11_serialmsg)
  add_erlang_bencherl_benchmark(bencherl_12_timer_wheel)

  add_custom_target(erlang_benchmarks
                    SOURCES ${CAF_ERLANG_SRC_FILES}
                    DEPENDS ${CAF_ERLANG_OUT_FILES})
  add_dependencies(all_benchmarks erlang_benchmarks)
  set(CAF_COMPILED_BENCHES "erlang ${CAF_COMPILED_BENCHES}")
else()
  add_dummy_target(erlang erl)
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
      message(STATUS "Disable Charm++ benchmarks: no running compiler found")
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
    set(CAF_CHARM_SRC_FILES ${longpath}.ci
                            ${longpath}.cpp ${CAF_CHARM_SRC_FILES})
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
                       COMMAND ${CAF_CHARM_COMPILER} ${longpath}.o
                               -o ${exe_file} -O3 -language charm++
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
  set(CAF_COMPILED_BENCHES "charm ${CAF_COMPILED_BENCHES}")
else()
  add_dummy_target(charm cpp ci)
endif()


################################################################################
#                                     CAF                                      #
################################################################################

set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${INCLUDE_DIR}")

# add targets for CAF benchmarks
macro(add_benchmark name path)
  if("${ARGN}" STREQUAL "")
    set(new_name ${name})
  else()
    string(REPLACE ";" "_" post_fix "${ARGN}")
    string(TOLOWER ${post_fix} post_fix)
    set(new_name "${name}_${post_fix}")
  endif()
  add_executable(${new_name} ${path})
  target_link_libraries(${new_name} ${CAF_LIBRARIES} ${LD_FLAGS})
  target_compile_definitions(${new_name} PRIVATE ${ARGN})
  add_dependencies(all_benchmarks ${new_name})
endmacro()

macro(add_caf_benchmark name)
  add_benchmark(${name} ${CMAKE_CURRENT_SOURCE_DIR}/src/caf/${name}.cpp ${ARGN})
endmacro()

macro(add_caf_savina_benchmark name)
  add_benchmark(${name} ${CMAKE_CURRENT_SOURCE_DIR}/src/caf/savina/${name}.cpp ${ARGN})
endmacro()

macro(add_caf_numa_benchmark name)
  add_benchmark(${name} ${CMAKE_CURRENT_SOURCE_DIR}/src/caf/numa/${name}.cpp ${ARGN})
endmacro()

macro(add_caf_bencherl_benchmark name)
  add_benchmark(${name} ${CMAKE_CURRENT_SOURCE_DIR}/src/caf/bencherl/${name}.cpp ${ARGN})
endmacro()

add_caf_benchmark(actor_creation)
add_caf_benchmark(mailbox_performance)
add_caf_benchmark(mixed_case)
add_caf_benchmark(mandelbrot)
add_caf_benchmark(micro)
#add_caf_benchmark(distributed)
add_caf_benchmark(matrix)
add_caf_benchmark(matching)
add_caf_benchmark(scheduling)

add_caf_savina_benchmark(savina_01_pingpong)
add_caf_savina_benchmark(savina_02_count)
add_caf_savina_benchmark(savina_03_fjthrput)
add_caf_savina_benchmark(savina_04_fjcreate)
add_caf_savina_benchmark(savina_05_threadring)
add_caf_savina_benchmark(savina_06_chameneos)
add_caf_savina_benchmark(savina_07_big)
add_caf_savina_benchmark(savina_08_concdict)
add_caf_savina_benchmark(savina_09_concsll)
add_caf_savina_benchmark(savina_10_bndbuffer)
add_caf_savina_benchmark(savina_11_philosopher)
#add_caf_savina_benchmark(savina_12_barber) bench uses busy waiting
#add_caf_savina_benchmark(savina_13_cigsmok) bench uses busy waiting
add_caf_savina_benchmark(savina_14_logmap REQUEST_AWAIT INFINITE)
add_caf_savina_benchmark(savina_14_logmap REQUEST_AWAIT HIGH_TIMEOUT)
add_caf_savina_benchmark(savina_14_logmap BECOME_UNBECOME_SLOW)
add_caf_savina_benchmark(savina_14_logmap BECOME_UNBECOME_FAST)
add_caf_savina_benchmark(savina_15_banking REQUEST_AWAIT INFINITE)
add_caf_savina_benchmark(savina_15_banking REQUEST_AWAIT HIGH_TIMEOUT)
add_caf_savina_benchmark(savina_15_banking REQUEST_THEN INFINITE)
add_caf_savina_benchmark(savina_15_banking REQUEST_THEN HIGH_TIMEOUT)
add_caf_savina_benchmark(savina_15_banking BECOME_UNBECOME_SLOW)
add_caf_savina_benchmark(savina_15_banking BECOME_UNBECOME_FAST)
add_caf_savina_benchmark(savina_16_radixsort)
#add_caf_savina_benchmark(savina_17_filterbank) savina implementation is buggy 
add_caf_savina_benchmark(savina_18_sieve)
#add_caf_savina_benchmark(savina_19_uct) bench uses busy waiting
add_caf_savina_benchmark(savina_20_facloc)
add_caf_savina_benchmark(savina_21_trapezoid)
#add_caf_savina_benchmark(savina_22_piprecision) bench uses BigDecimal
add_caf_savina_benchmark(savina_23_recmatmul)
add_caf_savina_benchmark(savina_24_quicksort)
add_caf_savina_benchmark(savina_25_apsp)
#add_caf_savina_benchmark(savina_26_sor) savina implementation is buggy
#add_caf_savina_benchmark(savina_27_astar) bench uses busy waiting
add_caf_savina_benchmark(savina_28_nqueenk)
add_caf_savina_benchmark(savina_29_fib)
add_caf_savina_benchmark(savina_30_bitonicsort)

add_caf_numa_benchmark(numa_matrix_search)

add_caf_bencherl_benchmark(bencherl_01_bang)
add_caf_bencherl_benchmark(bencherl_02_big)
add_caf_bencherl_benchmark(bencherl_03_ehb)
add_caf_bencherl_benchmark(bencherl_05_genstress)
add_caf_bencherl_benchmark(bencherl_06_mbrot)
add_caf_bencherl_benchmark(bencherl_08_parallel)
add_caf_bencherl_benchmark(bencherl_10_ran)
add_caf_bencherl_benchmark(bencherl_11_serialmsg)
add_caf_bencherl_benchmark(bencherl_12_timer_wheel)

set(CAF_COMPILED_BENCHES "caf ${CAF_COMPILED_BENCHES}")


################################################################################
#                          some environment variables                          #
################################################################################

set(TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools")
set(SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/scripts")


################################################################################
#                                    tools                                     #
################################################################################

find_package(Boost COMPONENTS system filesystem)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  add_executable(to_csv "${TOOLS_DIR}/to_csv.cpp")
  set(LD_FLAGS ${LD_FLAGS} ${Boost_SYSTEM_LIBRARY})
  set(LD_FLAGS ${LD_FLAGS} ${Boost_FILESYSTEM_LIBRARY})
  target_link_libraries(to_csv ${LD_FLAGS})
endif()

if (WIN32)
  message(STATUS "skip caf_run_bench (not supported on Windows)")
else()
  add_executable(caf_run_bench "${TOOLS_DIR}/caf_run_bench.cpp")
  target_link_libraries(caf_run_bench ${CAF_LIBRARIES} ${LD_FLAGS})
  add_dependencies(all_benchmarks caf_run_bench)
  add_custom_target(caf_scripts_dummy SOURCES "${SCRIPTS_DIR}/run")
endif()

################################################################################
#                                   Scripts                                    #
################################################################################

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/scripts/caf_run_benchmarks.in"
               "${SCRIPTS_DIR}/caf_run_benchmarks" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/scripts/run.in"
               "${SCRIPTS_DIR}/run" @ONLY)
