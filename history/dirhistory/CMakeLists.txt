project (monsoon_dirhistory)

find_path (STD_FILESYSTEM NAMES filesystem)
mark_as_advanced (STD_FILESYSTEM)
if (STD_FILESYSTEM)
else ()
  find_package (Boost COMPONENTS filesystem REQUIRED)
endif ()

include_directories (include)
include_directories ($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
include_directories ($<TARGET_PROPERTY:monsoon_misc,INTERFACE_INCLUDE_DIRECTORIES>)
include_directories ($<TARGET_PROPERTY:monsoon_history,INTERFACE_INCLUDE_DIRECTORIES>)
if (NOT STD_FILESYSTEM)
  include_directories (${Boost_INCLUDE_DIR})
endif ()
add_library (monsoon_dirhistory SHARED
  src/dirhistory.cc
  src/tsdata_mime.cc
)
target_link_libraries (monsoon_dirhistory ${extra_libs})
target_link_libraries (monsoon_dirhistory monsoon_intf monsoon_history)
if (NOT STD_FILESYSTEM)
  target_link_libraries (monsoon_dirhistory ${Boost_FILESYSTEM_LIBRARY})
endif ()
set_property (TARGET monsoon_history PROPERTY VERSION "0.0")
set_property (TARGET monsoon_history PROPERTY SOVERSION "0.0")
