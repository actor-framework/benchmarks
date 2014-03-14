cmake_minimum_required(VERSION 2.6)
project(cppa_benchmarks CXX)

add_custom_target(all_benchmarks)
include_directories(.)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

find_package(LibCPPA REQUIRED)
include_directories(. .. ${CPPA_INCLUDE})

if (CPPA_VERSION VERSION_LESS 0.9)
  set(CPPA_FILE_SUFFIX "_000801")
else ()
  set(CPPA_FILE_SUFFIX "")
endif ()

if (CPPA_LOG_LEVEL)
  add_definitions(-DCPPA_LOG_LEVEL=${CPPA_LOG_LEVEL})
endif(CPPA_LOG_LEVEL)

message("-- running on libcppa version ${CPPA_VERSION}: use suffix \"${CPPA_FILE_SUFFIX}\"")

macro(add_benchmark name)
  add_executable(${name} ${name}${CPPA_FILE_SUFFIX}.cpp)
  # LD_FLAGS set in main CMake file
  target_link_libraries(${name} ${CMAKE_DL_LIBS} ${CPPA_LIBRARY} ${LD_FLAGS})
  add_dependencies(${name} all_benchmarks)
endmacro()

add_benchmark(actor_creation)
add_benchmark(mailbox_performance)
add_benchmark(mixed_case)
add_benchmark(distributed)
add_benchmark(matching)
add_benchmark(matrix)

# done (print summary)
message("\n==================| CPPA Build Summary |==================="
        "\n"
        "\nLibcppa version:   ${CPPA_VERSION}"
        "\n"
        "\nBuild type:        ${CMAKE_BUILD_TYPE}"
        "\nLog level:         ${CPPA_LOG_LEVEL}"
        "\n"
        "\nCXX:               ${CMAKE_CXX_COMPILER}"
        "\nCXXFLAGS:          ${CMAKE_CXX_FLAGS}"
        "\n"
        "\nSource directory:  ${CMAKE_SOURCE_DIR}"
        "\nBuild directory:   ${CMAKE_BINARY_DIR}"
        "\nExecutable path:   ${EXECUTABLE_OUTPUT_PATH}"
        "\nLibrary path:      ${LIBRARY_OUTPUT_PATH}"
        "\nInstall prefix:    ${CMAKE_INSTALL_PREFIX}"
        "\n"
        "\n===========================================================\n")
