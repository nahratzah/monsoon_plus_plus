project (monsoon_history)

add_library (monsoon_history
  src/collect_history.cc
  src/print_history.cc
)
target_include_directories (monsoon_history PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries (monsoon_history PUBLIC monsoon_intf)
set_property (TARGET monsoon_history PROPERTY VERSION "0.0")
set_property (TARGET monsoon_history PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_history DESTINATION lib)
install (FILES
  include/monsoon/history/history_export_.h
  include/monsoon/history/collect_history.h
  include/monsoon/history/print_history.h
  DESTINATION include/monsoon/history)

add_subdirectory (dirhistory)
