# -- get dependencies ----------------------------------------------------------

if(POLICY CMP0074)
  set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
  cmake_policy(SET CMP0074 NEW)
endif()

find_package(CAF COMPONENTS core io REQUIRED)

if(CAF_VERSION VERSION_GREATER_EQUAL 0.18)
  set(CAF_LIBRARIES CAF::core CAF::io)
endif()

# -- utility macros ------------------------------------------------------------

function(add_caf_benchmark name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} ${CAF_LIBRARIES} ${LD_FLAGS})
  add_dependencies(all_benchmarks ${name})
endfunction()

# -- benchmark programs --------------------------------------------------------

foreach(name
          "actor_creation" "mailbox_performance" "mixed_case" "mandelbrot"
          "matrix" "matching" "scheduling" "simple_streaming")
  add_caf_benchmark("${name}")
endforeach()

#add_caf_benchmark(distributed)

# -- micro benchmarks ----------------------------------------------------------

add_subdirectory(microbenchmarks)

# -- tools ---------------------------------------------------------------------

if (WIN32)
  message(STATUS "skip caf_run_bench (not supported on Windows)")
else()
  add_executable(caf_run_bench "${TOOLS_DIR}/caf_run_bench.cpp")
  target_link_libraries(caf_run_bench ${CAF_LIBRARIES} ${LD_FLAGS})
  add_dependencies(all_benchmarks caf_run_bench)
  add_custom_target(caf_scripts_dummy SOURCES "${SCRIPTS_DIR}/run")
endif()
