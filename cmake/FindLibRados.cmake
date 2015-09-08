FIND_PATH(RADOS_INCLUDE_DIR librados.hpp
  HINTS
  $ENV{XROOTD_DIR}
  /usr/local/
  /usr
  /opt/rados/
  PATH_SUFFIXES include/rados
  PATHS /opt/rados
)

FIND_LIBRARY(RADOS_LIB librados rados
  HINTS
  /usr/local/
  /usr
  /opt/rados/
  PATH_SUFFIXES lib
  lib64
  lib/rados/
  lib64/rados/
)

set( RADOS_LIB_VERSION_MAJOR 0 )
set( RADOS_LIB_VERSION_MINOR 0 )
set( RADOS_LIB_VERSION_PATCH 0 )
set( RADOS_LIB_VERSION ${RADOS_LIB_VERSION_MAJOR}.${RADOS_LIB_VERSION_MINOR}.${RADOS_LIB_VERSION_PATCH} )

if( RADOS_LIB )
  execute_process( COMMAND rados --version OUTPUT_VARIABLE RADOS_VERSION )
  string( REPLACE " " ";" RADOS_VERSION_LIST ${RADOS_VERSION} )
  list( GET RADOS_VERSION_LIST 2 RADOS_VERSION )
  string( REPLACE "." ";" RADOS_VERSION ${RADOS_VERSION} )
  list( GET RADOS_VERSION 0 RADOS_LIB_VERSION_MAJOR )
  list( GET RADOS_VERSION 1 RADOS_LIB_VERSION_MINOR )
  list( GET RADOS_VERSION 2 RADOS_LIB_VERSION_PATCH )
  message( STATUS "Found RADOS version ${RADOS_LIB_VERSION_MAJOR}.${RADOS_LIB_VERSION_MINOR}.${RADOS_LIB_VERSION_PATCH}" )
endif( RADOS_LIB )

# GET_FILENAME_COMPONENT( XROOTD_LIB_DIR ${XROOTD_UTILS} PATH )

INCLUDE( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( Ceph DEFAULT_MSG
                                        RADOS_LIB
                                        RADOS_INCLUDE_DIR )
