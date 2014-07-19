cmake_minimum_required(VERSION 2.6)
project(cppa_benchmarks CXX)

add_custom_target(all_benchmarks)
include_directories(.)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Libcaf COMPONENTS core io REQUIRED)

include_directories(. .. ${LIBCAF_INCLUDE_DIR_CORE} ${LIBCAF_INCLUDE_DIR_IO})

if (LIBCAF_LOG_LEVEL)
  add_definitions(-DLIBCAF_LOG_LEVEL=${LIBCAF_LOG_LEVEL})
endif(LIBCAF_LOG_LEVEL)

macro(add_benchmark name)
  add_executable(${name} ${name}.cpp)
  # LD_FLAGS set in main CMake file
  target_link_libraries(${name}
                        ${CMAKE_DL_LIBS}
                        ${LIBCAF_LIBRARY_CORE}
                        ${LIBCAF_LIBRARY_IO})
  add_dependencies(${name} all_benchmarks)
endmacro()

add_benchmark(actor_creation)
add_benchmark(mailbox_performance)
add_benchmark(mixed_case)
#add_benchmark(distributed)
#add_benchmark(matching)
#add_benchmark(matrix)

# done (print summary)
message("\n==================| CPPA Build Summary |==================="
        "\n"
        "\nLibcaf_core include:   ${LIBCAF_INCLUDE_DIR_CORE}"
        "\nLibcaf_core lib:       ${LIBCAF_LIBRARY_CORE}"
        "\n"
        "\nLibcaf_io include:     ${LIBCAF_INCLUDE_DIR_IO}"
        "\nLibcaf_io lib:         ${LIBCAF_LIBRARY_IO}"
        "\n"
        "\nBuild type:            ${CMAKE_BUILD_TYPE}"
        "\nLog level:             ${LIBCAF_LOG_LEVEL}"
        "\n"
        "\nCXX:                   ${CMAKE_CXX_COMPILER}"
        "\nCXXFLAGS:              ${CMAKE_CXX_FLAGS}"
        "\n"
        "\nSource directory:      ${CMAKE_SOURCE_DIR}"
        "\nBuild directory:       ${CMAKE_BINARY_DIR}"
        "\nExecutable path:       ${EXECUTABLE_OUTPUT_PATH}"
        "\nLibrary path:          ${LIBRARY_OUTPUT_PATH}"
        "\nInstall prefix:        ${CMAKE_INSTALL_PREFIX}"
        "\n"
        "\n===========================================================\n")
