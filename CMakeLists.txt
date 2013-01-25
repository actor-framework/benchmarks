cmake_minimum_required (VERSION 2.8.6)
project (benchmarks)

set (CMAKE_CXX_FLAGS_RELEASE "-O3")
set (CMAKE_BUILD_TYPE RELEASE)

if (    "${DISABLED_BENCHMARKS}" MATCHES "cppa"
    AND "${DISABLED_BENCHMARKS}" MATCHES "theron")
else ()
  # check if the user provided CXXFLAGS on the command line
  if ("${CMAKE_CXX_FLAGS}" STREQUAL "")
    set (CXXFLAGS_PROVIDED false)
    set (CMAKE_CXX_FLAGS "-std=c++11 -Wextra -Wall -pedantic")
  else ()
    set (CXXFLAGS_PROVIDED true)
  endif ()
  # check for g++ >= 4.7 or clang++ > = 3.2
  try_run (ProgramResult
           CompilationSucceeded
           ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/get_compiler_version.cpp
           RUN_OUTPUT_VARIABLE CompilerVersion)
  if (NOT CompilationSucceeded OR NOT (ProgramResult EQUAL 0))
    message (FATAL_ERROR "Cannot determine compiler version")
  elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    if (CompilerVersion VERSION_GREATER 4.6)
      message (STATUS "Found g++ version ${CompilerVersion}")
    else ()
      message (FATAL_ERROR "g++ >= 4.7 required (found: ${CompilerVersion}.")
    endif ()
  elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    if (CompilerVersion VERSION_GREATER 3.1)
      message (STATUS "Found clang++ version ${CompilerVersion}")
    else ()
      message (FATAL_ERROR "clang++ >= 3.2 required (found: ${CompilerVersion}.")
    endif ()
    if  (NOT CXXFLAGS_PROVIDED)
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif ()
  else ()
    message (FATAL_ERROR "Your C++ compiler does not support C++11.")
  endif ()
endif ()

set (CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set (EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set (PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/.)

add_custom_target(benchmark_target ALL)

include_directories("${PROJECT_INCLUDE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}")

if ("${DISABLED_BENCHMARKS}" MATCHES "cppa")
  message (STATUS "skip libcppa benchmarks (disabled)")
else ()
  add_subdirectory(cppa)
endif ()

if ("${DISABLED_BENCHMARKS}" MATCHES "theron")
  message (STATUS "skip Theron benchmarks (disabled)")
else ()
  add_subdirectory(theron)
  add_dependencies(benchmark_target theron_benchmarks)
endif ()


if ("${DISABLED_BENCHMARKS}" MATCHES "scala")
  message (STATUS "skip Scala benchmarks (disabled)")
else ()
  # check for scalac and version
  if ("${SCALAC}" STREQUAL "")
    set (SCALAC "scalac")
  endif ()
  execute_process (COMMAND "${SCALAC}" "-version"
                   RESULT_VARIABLE ScalacResult
                   ERROR_VARIABLE ScalacVersionStr)
  if (ScalacResult EQUAL 0)
    message (STATUS "Scala compiler ${SCALAC} appears to be working")
  else ()
    message (FATAL_ERROR "Scala compiler ${SCALAC} not found")
  endif ()
  string (REGEX REPLACE ".*version ([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" ScalacVersion 
"${ScalacVersionStr}")
  if (ScalacVersion VERSION_LESS 2.10)
    message (FATAL_ERROR "Scala compiler in version >= 2.10 required, "
                         "found: ${ScalacVersion}")
  endif ()
  add_subdirectory(scala)
  add_dependencies(benchmark_target scalabench)
endif ()

if ("${DISABLED_BENCHMARKS}" MATCHES "erlang")
  message (STATUS "skip Erlang benchmarks (disabled)")
else()
  add_subdirectory(erlang)
  add_dependencies(benchmark_target erlangbench)
endif ()

if ("${DISABLED_BENCHMARKS}" MATCHES "golang")
  message (STATUS "skip Go benchmarks (disabled)")
else()
  add_subdirectory(go)
  add_dependencies(benchmark_target gobench)
endif ()
