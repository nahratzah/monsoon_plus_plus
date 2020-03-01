project (monsoon_tx)

include (CheckFunctionExists)

find_package (Boost COMPONENTS system REQUIRED)

add_library (monsoon_tx
  src/tx/txfile.cc
  src/tx/sequence.cc
  src/tx/db.cc
  src/tx/tree.cc
  src/tx/tree_error.cc
  src/tx/detail/replacement_map.cc
  src/tx/detail/tx_sequencer.cc
  src/tx/detail/wal.cc
  src/tx/detail/tree_page.cc
  src/tx/detail/tree_cfg.cc
  src/tx/tx_aware_data.cc
  src/tx/detail/commit_id.cc
)
target_include_directories (monsoon_tx PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_include_directories (monsoon_tx PUBLIC ${Boost_INCLUDE_DIR})
target_link_libraries (monsoon_tx PUBLIC
  monsoon_lib
  monsoon_misc
  instrumentation
  cycle_ptr
  ${Boost_SYSTEM_LIBRARY}
)
target_link_libraries (monsoon_tx PRIVATE
  objpipe
)
set_property (TARGET monsoon_tx PROPERTY VERSION "0.0")
set_property (TARGET monsoon_tx PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_tx DESTINATION lib)
install (FILES
  include/monsoon/tx/txfile.h
  include/monsoon/tx/sequence.h
  include/monsoon/tx/db.h
  include/monsoon/tx/db-inl.h
  include/monsoon/tx/tree.h
  include/monsoon/tx/tree-inl.h
  include/monsoon/tx/tree_error.h
  include/monsoon/tx/tx_aware_data.h
  DESTINATION include/monsoon/tx)
install (FILES
  include/monsoon/tx/detail/export_.h
  include/monsoon/tx/detail/replacement_map.h
  include/monsoon/tx/detail/tx_sequencer.h
  include/monsoon/tx/detail/wal.h
  include/monsoon/tx/detail/tree_spec.h
  include/monsoon/tx/detail/tree_page.h
  include/monsoon/tx/detail/tree_page-inl.h
  include/monsoon/tx/detail/tree_cfg.h
  include/monsoon/tx/detail/commit_id.h
  include/monsoon/tx/detail/commit_id-inl.h
  DESTINATION include/monsoon/tx/detail)

add_subdirectory (tests)
