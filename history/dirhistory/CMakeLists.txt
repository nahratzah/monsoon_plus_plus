project (monsoon_dirhistory)

find_path (STD_FILESYSTEM NAMES filesystem)
mark_as_advanced (STD_FILESYSTEM)

list (APPEND Boost_required_components context)
mark_as_advanced (Boost_required_components)
if (NOT STD_FILESYSTEM)
  list (APPEND Boost_required_components filesystem system)
endif ()
find_package (Boost COMPONENTS ${Boost_required_components} REQUIRED)

include_directories (include)
include_directories ($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
include_directories ($<TARGET_PROPERTY:monsoon_misc,INTERFACE_INCLUDE_DIRECTORIES>)
include_directories ($<TARGET_PROPERTY:monsoon_history,INTERFACE_INCLUDE_DIRECTORIES>)
if (NOT STD_FILESYSTEM)
  include_directories (${Boost_INCLUDE_DIR})
endif ()
add_library (monsoon_dirhistory
  src/raw_file_segment_reader.cc
  src/raw_file_segment_writer.cc
  src/hdir_exception.cc
  src/dirhistory.cc
  src/tsdata_mime.cc
  src/tsdata.cc
  src/v0/tsdata.cc
  src/v1/tsdata.cc
  src/v2/encdec.cc
  src/v2/tsdata.cc
  src/v2/tsdata_tables.cc
  src/v2/tsdata_list.cc
)
target_include_directories(monsoon_history PUBLIC include)
if (NOT STD_FILESYSTEM)
  target_include_directories(monsoon_history PUBLIC ${Boost_INCLUDE_DIR})
endif()
target_link_libraries (monsoon_dirhistory ${extra_libs})
target_link_libraries (monsoon_dirhistory
  monsoon_intf
  monsoon_misc
  monsoon_history)
if (NOT STD_FILESYSTEM)
  target_link_libraries (monsoon_dirhistory ${Boost_FILESYSTEM_LIBRARY})
  target_link_libraries (monsoon_dirhistory ${Boost_SYSTEM_LIBRARY})
endif ()
target_link_libraries (monsoon_dirhistory ${Boost_CONTEXT_LIBRARY})
target_link_libraries (monsoon_dirhistory ${Boost_COROUTINE2_LIBRARY})
set_property (TARGET monsoon_dirhistory PROPERTY VERSION "0.0")
set_property (TARGET monsoon_dirhistory PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_dirhistory DESTINATION lib)
install (FILES
  include/monsoon/history/dir/dirhistory.h
  include/monsoon/history/dir/hdir_exception.h
  include/monsoon/history/dir/dirhistory_export_.h
  include/monsoon/history/dir/tsdata.h
  DESTINATION include/monsoon/history/dir)

add_subdirectory (tests)
