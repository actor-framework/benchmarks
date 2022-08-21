# -- get dependencies ----------------------------------------------------------

set(DISABLE_STREAMING_BENCH OFF)

if (CAF_ROOT)
  find_package(CAF COMPONENTS core io REQUIRED)
else()
  message(STATUS "Fetch CAF ${CAF_TAG}")
  FetchContent_Declare(
    actor_framework
    GIT_REPOSITORY https://github.com/actor-framework/actor-framework.git
    GIT_TAG        ${CAF_TAG}
  )
  FetchContent_Populate(actor_framework)
  string(REGEX MATCH "^[0-9]+\.[0-9]+\.[0-9]+$" TAG_IS_VERSION "${CAF_TAG}")
  if(NOT TAG_IS_VERSION OR NOT CAF_TAG VERSION_LESS 0.18.0)
    # CAF >= 0.18 setup
    foreach(varname CAF_ENABLE_EXAMPLES CAF_ENABLE_TESTING
                    CAF_ENABLE_TOOLS CAF_ENABLE_OPENSSL_MODULE)
      set(${varname} OFF CACHE INTERNAL "")
    endforeach()
    set(CAF_SANITIZERS "${SANITIZERS}" CACHE INTERNAL "")
    add_subdirectory(${actor_framework_SOURCE_DIR} ${actor_framework_BINARY_DIR})
  else()
    if(CAF_TAG VERSION_LESS 0.17.0)
      set(DISABLE_STREAMING_BENCH ON)
    endif()
    # CAF < 0.18 setup
    foreach(varname CAF_NO_COMPILER_CHECK CAF_NO_AUTO_LIBCPP CAF_NO_EXAMPLES
                    CAF_NO_UNIT_TESTS CAF_NO_OPENSSL CAF_NO_OPENCL CAF_NO_TOOLS
                    CAF_NO_PYTHON CAF_NO_SUMMARY)
      set(${varname} ON CACHE INTERNAL "")
    endforeach()
    if(NOT BUILD_SHARED_LIBS)
      set(CAF_BUILD_STATIC_ONLY ON CACHE INTERNAL "")
    endif()
    add_subdirectory(${actor_framework_SOURCE_DIR} ${actor_framework_BINARY_DIR})
    if(BUILD_SHARED_LIBS)
      target_include_directories(libcaf_core_shared INTERFACE
                                 "${actor_framework_SOURCE_DIR}/libcaf_core")
      target_include_directories(libcaf_io_shared INTERFACE
                                 "${actor_framework_SOURCE_DIR}/libcaf_io")
      add_library(CAF::core ALIAS libcaf_core_shared)
      add_library(CAF::io ALIAS libcaf_io_shared)
    else()
      target_include_directories(libcaf_core_static INTERFACE
                                 "${actor_framework_SOURCE_DIR}/libcaf_core")
      target_include_directories(libcaf_io_static INTERFACE
                                 "${actor_framework_SOURCE_DIR}/libcaf_io")
      add_library(CAF::core ALIAS libcaf_core_static)
      add_library(CAF::io ALIAS libcaf_io_static)
    endif()
  endif()
endif()

# -- utility macros ------------------------------------------------------------

function(add_caf_benchmark name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} CAF::core CAF::io ${LD_FLAGS})
  add_dependencies(all_benchmarks ${name})
endfunction()

# -- benchmark programs --------------------------------------------------------

foreach(name
          "actor_creation" "mailbox_performance" "mixed_case" "mandelbrot"
          "matching" "scheduling")
  add_caf_benchmark("${name}")
endforeach()

#add_caf_benchmark(distributed)

# -- tools ---------------------------------------------------------------------

if (WIN32)
  message(STATUS "skip caf_run_bench (not supported on Windows)")
else()
  add_executable(caf_run_bench "${TOOLS_DIR}/caf_run_bench.cpp")
  target_link_libraries(caf_run_bench CAF::core CAF::io ${LD_FLAGS})
  add_dependencies(all_benchmarks caf_run_bench)
  add_custom_target(caf_scripts_dummy SOURCES "${SCRIPTS_DIR}/run")
endif()
