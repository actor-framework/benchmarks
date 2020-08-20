find_package(benchmark)

if(NOT benchmark_FOUND)
  return()
endif()

function(add_caf_microbenchmark name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} ${CAF_LIBRARIES} ${LD_FLAGS} benchmark::benchmark_main)
  add_dependencies(all_benchmarks ${name})
endfunction()

foreach(name "message-creation" "pattern-matching" "serialization" "streaming")
  add_caf_microbenchmark("${name}")
endforeach()
