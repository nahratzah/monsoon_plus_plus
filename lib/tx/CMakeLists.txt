project (monsoon_tx)

include (CheckFunctionExists)

find_package (Boost REQUIRED)

add_library (monsoon_tx
  src/tx/txfile.cc
  src/tx/sequence.cc
  src/tx/detail/replacement_map.cc
  src/tx/detail/tx_sequencer.cc
  src/tx/detail/wal.cc
)
target_include_directories (monsoon_tx PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_include_directories (monsoon_tx PUBLIC ${Boost_INCLUDE_DIR})
target_link_libraries (monsoon_tx PUBLIC
  monsoon_misc
  instrumentation
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
  DESTINATION include/monsoon/tx)
install (FILES
  include/monsoon/tx/detail/export_.h
  include/monsoon/tx/detail/replacement_map.h
  include/monsoon/tx/detail/tx_sequencer.h
  include/monsoon/tx/detail/wal.h
  DESTINATION include/monsoon/tx/detail)

add_subdirectory (tests)
