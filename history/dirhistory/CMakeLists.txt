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
add_library (monsoon_dirhistory
  src/dirhistory.cc
  src/tsdata_mime.cc
  src/v0/tsdata.cc
  src/tsdata.cc
)
target_link_libraries (monsoon_dirhistory ${extra_libs})
target_link_libraries (monsoon_dirhistory
  monsoon_intf
  monsoon_misc
  monsoon_history)
if (NOT STD_FILESYSTEM)
  target_link_libraries (monsoon_dirhistory ${Boost_FILESYSTEM_LIBRARY})
endif ()
set_property (TARGET monsoon_dirhistory PROPERTY VERSION "0.0")
set_property (TARGET monsoon_dirhistory PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_dirhistory DESTINATION lib)
install (FILES
  include/monsoon/history/dir/dirhistory.h
  include/monsoon/history/dir/dirhistory_export_.h
  include/monsoon/history/dir/tsdata.h
  DESTINATION include/monsoon/history/dir)
