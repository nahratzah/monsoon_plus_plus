project (monsoon_dirhistory)

find_path (STD_FILESYSTEM NAMES filesystem)
mark_as_advanced (STD_FILESYSTEM)

list (APPEND Boost_required_components)
mark_as_advanced (Boost_required_components)
if (NOT STD_FILESYSTEM)
  list (APPEND Boost_required_components filesystem system)
endif ()
find_package (Boost COMPONENTS ${Boost_required_components} REQUIRED)

add_library (monsoon_dirhistory
  src/raw_file_segment_reader.cc
  src/raw_file_segment_writer.cc
  src/hdir_exception.cc
  src/dirhistory.cc
  src/tsdata_mime.cc
  src/tsdata.cc
  src/dynamics.cc
  src/v0/tsdata.cc
  src/v1/tsdata.cc
  src/v2/encdec_ctx.cc
  src/v2/xdr_primitives.cc
  src/v2/timestamp_delta.cc
  src/v2/dictionary.cc
  src/v2/metric_table.cc
  src/v2/group_table.cc
  src/v2/tables.cc
  src/v2/file_data_tables_block.cc
  src/v2/encdec.cc
  src/v2/tsdata.cc
  src/v2/tsdata_tables.cc
  src/v2/tsdata_list.cc
  src/v2/bitset.cc
)
target_include_directories (monsoon_dirhistory PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries (monsoon_dirhistory PUBLIC
  monsoon_intf
  monsoon_misc
  monsoon_history
  objpipe)
if (NOT STD_FILESYSTEM)
  target_include_directories (monsoon_dirhistory PUBLIC ${Boost_INCLUDE_DIR})
  target_link_libraries (monsoon_dirhistory PUBLIC ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY})
endif ()
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
