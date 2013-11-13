cmake_minimum_required(VERSION 2.6)
project(cppa_benchmarks CXX)

add_custom_target(all_tools)
include_directories(.)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Boost REQUIRED)
include_directories(. .. ${Boost_INCLUDE_DIRS})

macro(add_tool name)
  add_executable(${name} ${name}.cpp)
  # LD_FLAGS set in main CMake file
  target_link_libraries(${name} ${CMAKE_DL_LIBS} ${CPPA_LIBRARY} ${LD_FLAGS})
  add_dependencies(${name} all_tools)
endmacro()

add_tool(to_dat)
