# - Try to find libcppa
# Once done this will define
#
#  CPPA_FOUND    - system has libcppa
#  CPPA_INCLUDE  - libcppa include dir
#  CPPA_LIBRARY  - link againgst libcppa
#  CPPA_VERSION  - version in {major}.{minor}.{patch} format

if (CPPA_LIBRARY AND CPPA_INCLUDE)
  set(CPPA_FOUND TRUE)
else (CPPA_LIBRARY AND CPPA_INCLUDE)

  find_path(CPPA_INCLUDE
    NAMES
      cppa/cppa.hpp
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
      ${CPPA_INCLUDE_PATH}
      ${CPPA_LIBRARY_PATH}
      ${CMAKE_INCLUDE_PATH}
      ${CMAKE_INSTALL_PREFIX}/include
      ${CPPA_ROOT}/include
      ${CPPA_ROOT}/libcppa
      ../libcppa
      ../../libcppa
      ../../../libcppa
  )
  
  if (CPPA_INCLUDE) 
    message (STATUS "Header files found ...")
  else (CPPA_INCLUDE)
    message (SEND_ERROR "Header files NOT found. Provide absolute path with -DCPPA_INCLUDE_PATH=<path-to-header>.")
  endif (CPPA_INCLUDE)

  if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    set(CPPA_BUILD_DIR build-gcc)
  elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CPPA_BUILD_DIR build-clang)
  endif ()

  find_library(CPPA_LIBRARY
    NAMES
      libcppa
      cppa
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
      ${CPPA_INCLUDE_PATH}
      ${CPPA_LIBRARY_PATH}
      ${CMAKE_LIBRARY_PATH}
      ${CMAKE_INSTALL_PREFIX}/lib
      ${LIBRARY_OUTPUT_PATH}
      ${CPPA_ROOT}/lib
      ${CPPA_ROOT}/build/lib
      ${CPPA_ROOT}/libcppa/build/lib
      ${CPPA_ROOT}/${CPPA_BUILD_DIR}/lib
      ${CPPA_ROOT}/libcppa/${CPPA_BUILD_DIR}/lib
      ../libcppa/build/lib
      ../../libcppa/build/lib
      ../../../libcppa/build/lib
      ../libcppa/${CPPA_BUILD_DIR}/lib
      ../../libcppa/${CPPA_BUILD_DIR}/lib
      ../../../libcppa/${CPPA_BUILD_DIR}/lib
  )

  if (CPPA_LIBRARY) 
    message (STATUS "Library found ...")
  else (CPPA_LIBRARY)
    message (SEND_ERROR "Library NOT found. Provide absolute path with -DCPPA_LIBRARY_PATH=<path-to-library>.")
  endif (CPPA_LIBRARY)

  if (CPPA_INCLUDE AND CPPA_LIBRARY)
    set(CPPA_FOUND TRUE)
    set(CPPA_INCLUDE ${CPPA_INCLUDE})
    set(CPPA_LIBRARY ${CPPA_LIBRARY})
  else (CPPA_INCLUDE AND CPPA_LIBRARY)
    message (FATAL_ERROR "CPPA LIBRARY AND/OR HEADER NOT FOUND!")
  endif (CPPA_INCLUDE AND CPPA_LIBRARY)

  # extract CPPA_VERSION from config.hpp
  if (CPPA_INCLUDE)
    # we assume version 0.8.1 if CPPA_VERSION is not defined config.hpp
    set(CPPA_VERSION 801)
    file(READ "${CPPA_INCLUDE}/cppa/config.hpp" CPPA_CONFIG_HPP_CONTENT)
    string(REGEX REPLACE ".*#define CPPA_VERSION ([0-9]+).*" "\\1" CPPA_VERSION "${CPPA_CONFIG_HPP_CONTENT}")
    if ("${CPPA_VERSION}" MATCHES "^[0-9]+$")
      math(EXPR CPPA_VERSION_MAJOR "${CPPA_VERSION} / 100000")
      math(EXPR CPPA_VERSION_MINOR "${CPPA_VERSION} / 100 % 1000")
      math(EXPR CPPA_VERSION_PATCH "${CPPA_VERSION} % 100")
      set(CPPA_VERSION "${CPPA_VERSION_MAJOR}.${CPPA_VERSION_MINOR}.${CPPA_VERSION_PATCH}")
    else ()
      set(CPPA_VERSION "0.8.1")
    endif ()
  endif (CPPA_INCLUDE)

endif (CPPA_LIBRARY AND CPPA_INCLUDE)
