# ZLIB_FOUND
# ZLIB_INCLUDE_DIR
# ZLIB_LIBRARY

if (WIN32)
  find_path (ZLIB_INCLUDE_DIR zlib.h
             ${ZLIB_ROOT_PATH}/include
             ${PROJECT_SOURCE_DIR}/include
             doc "The directory where zlib.h header resides")
  find_library (ZLIB_LIBRARY
                NAMES zlib
                PATHS
                ${PROJECT_SOURCE_DIR}/lib
                DOC "The zlib library")
else (WIN32)
  find_path (ZLIB_INCLUDE_DIR zlib.h
             ~/include
             /usr/include
             /usr/local/include)
  find_library (ZLIB_LIBRARY z
                ~/lib
                /usr/lib
                /usr/local/lib)
endif (WIN32)

if (ZLIB_INCLUDE_DIR)
  set (ZLIB_FOUND 1 CACHE STRING "set to 1 if zlib is found, 0 otherwise")
else (ZLIB_INCLUDE_DIR)
  set (ZLIB_FOUND 0 CACHE STRING "set to 1 if zlib is found, 0 otherwise")
endif (ZLIB_INCLUDE_DIR)

mark_as_advanced (ZLIB_FOUND)

