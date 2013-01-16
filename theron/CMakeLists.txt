cmake_minimum_required(VERSION 2.6)
project(theron_benchmarks CXX)

add_custom_target(theron_benchmarks)

find_package(Boost REQUIRED)

if ("${THERON_ROOT}" STREQUAL "")
  message(FATAL_ERROR "sorry, Theron without using --with-theron=PATH not supported yet")
else ()
  set (THERON_INCLUDE "${THERON_ROOT}/Include" "${THERON_ROOT}/External")
  set (THERON_LIB "${THERON_ROOT}/Lib/libtheron.a")
endif ()

include_directories(. .. ${THERON_INCLUDE} ${Boost_INCLUDE_DIRS})

macro(add_theron_benchmark shortname)
  set(name "theron_${shortname}")
  add_executable(${name} ${shortname}.cpp)
  target_link_libraries(${name} ${CMAKE_DL_LIBS} ${THERON_LIB})
  add_dependencies(${name} theron_benchmarks)
endmacro()

add_theron_benchmark(actor_creation)
add_theron_benchmark(mailbox_performance)
add_theron_benchmark(mixed_case)
