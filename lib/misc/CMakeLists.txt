project (monsoon_misc)

include (CheckFunctionExists)

find_package (Boost REQUIRED)
find_package (ZLIB REQUIRED)

find_path (STD_FILESYSTEM NAMES filesystem)
mark_as_advanced (STD_FILESYSTEM)
if (STD_FILESYSTEM)
  add_definitions(-DHAS_CXX_FILESYSTEM)
else ()
  find_package (Boost COMPONENTS filesystem REQUIRED)
endif ()

if (NOT WIN32)
  CHECK_FUNCTION_EXISTS (mkstemp HAS_MKSTEMP)
  mark_as_advanced (HAS_MKSTEMP)
  if (HAS_MKSTEMP)
    add_definitions(-DHAS_MKSTEMP)
  else ()
    message (WARNING "no mkstemp() function found")
  endif ()
endif ()

include_directories (include)
include_directories (${Boost_INCLUDE_DIR})
include_directories (${ZLIB_INCLUDE_DIRS})
include_directories ($<TARGET_PROPERTY:monsoon_lib,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_misc SHARED
  src/fd.cc
  src/stream.cc
  src/positional_stream.cc
  src/xdr.cc
  src/gzip_stream.cc
)
target_include_directories(monsoon_misc PUBLIC include)
target_link_libraries (monsoon_misc monsoon_lib)
if (NOT STD_FILESYSTEM)
  target_link_libraries (monsoon_misc ${Boost_FILESYSTEM_LIBRARY})
endif ()
target_link_libraries (monsoon_misc ${ZLIB_LIBRARIES})
set_property (TARGET monsoon_lib PROPERTY VERSION "0.0")
set_property (TARGET monsoon_lib PROPERTY SOVERSION "0.0")
