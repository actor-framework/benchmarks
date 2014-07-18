# - Try to find libcppa
# Once done this will define
#
#  CPPA_FOUND    - system has libcppa
#  CPPA_INCLUDE  - libcppa include dir
#  CPPA_LIBRARY  - link againgst libcppa
#  CPPA_VERSION  - version in {major}.{minor}.{patch} format

find_path(LIBCAF_INCLUDE_DIR
          NAMES
            caf/all.hpp
          HINTS
            ${LIBCAF_ROOT_DIR}/include
            /usr/include
            /usr/local/include
            /opt/local/include
            /sw/include
            ${CMAKE_INSTALL_PREFIX}/include
            ../libcppa
            ../../libcppa
            ../../../libcppa)

find_library(LIBCAF_LIBRARIES
             NAMES
               caf
             HINTS
               ${LIBCAF_ROOT_DIR}/lib
               /usr/lib
               /usr/local/lib
               /opt/local/lib
               /sw/lib
               ${CMAKE_INSTALL_PREFIX}/lib
               ../libcppa/build/lib
               ../../libcppa/build/lib
               ../../../libcppa/build/lib)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(libcaf DEFAULT_MSG
  LIBCAF_LIBRARIES
  LIBCAF_INCLUDE_DIR)

mark_as_advanced(
  LIBCAF_ROOT_DIR
  LIBCAF_LIBRARIES
  LIBCAF_INCLUDE_DIR)
