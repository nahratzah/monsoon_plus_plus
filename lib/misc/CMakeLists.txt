project (monsoon_misc)

find_package (Boost REQUIRED)

find_path (STD_FILESYSTEM NAMES filesystem)
mark_as_advanced (STD_FILESYSTEM)
if (STD_FILESYSTEM)
  add_definitions(-DHAS_CXX_FILESYSTEM)
else ()
  find_package (Boost COMPONENTS filesystem REQUIRED)
endif ()

include_directories (include)
include_directories (${Boost_INCLUDE_DIR})
add_library (monsoon_misc SHARED
  src/fd.cc
  src/xdr.cc
)
target_include_directories(monsoon_misc PUBLIC include)
if (NOT STD_FILESYSTEM)
  target_link_libraries (monsoon_misc ${Boost_FILESYSTEM_LIBRARY})
endif ()
set_property (TARGET monsoon_lib PROPERTY VERSION "0.0")
set_property (TARGET monsoon_lib PROPERTY SOVERSION "0.0")
