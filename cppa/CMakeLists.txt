cmake_minimum_required(VERSION 2.6)
project(cppa_benchmarks CXX)

add_custom_target(all_benchmarks)
include_directories(.)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

find_package(LibCPPA REQUIRED)
find_package(Boost REQUIRED)
include_directories(. .. ${CPPA_INCLUDE} ${Boost_INCLUDE_DIRS})

macro(add_benchmark name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} ${CMAKE_DL_LIBS} ${CPPA_LIBRARY})
  add_dependencies(${name} all_benchmarks)
endmacro()

add_benchmark(actor_creation)
add_benchmark(mailbox_performance)
add_benchmark(mixed_case)
add_benchmark(distributed)
add_benchmark(matching)
