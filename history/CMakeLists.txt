project (monsoon_history)

include_directories (include)
include_directories ($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_history
  src/collect_history.cc
  src/instrumentation.cc
)
target_include_directories(monsoon_history PUBLIC include)
target_link_libraries (monsoon_history ${extra_libs})
target_link_libraries (monsoon_history monsoon_intf)
set_property (TARGET monsoon_history PROPERTY VERSION "0.0")
set_property (TARGET monsoon_history PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_history DESTINATION lib)
install (FILES
  include/monsoon/history/history_export_.h
  include/monsoon/history/collect_history.h
  DESTINATION include/monsoon/history)

add_subdirectory (dirhistory)
