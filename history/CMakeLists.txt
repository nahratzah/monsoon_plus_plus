project (monsoon_history)

include_directories (include)
include_directories ($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_history SHARED
  src/collect_history.cc
)
target_include_directories(monsoon_history PUBLIC include)
target_link_libraries (monsoon_history ${extra_libs})
target_link_libraries (monsoon_history monsoon_intf)
set_property (TARGET monsoon_history PROPERTY VERSION "0.0")
set_property (TARGET monsoon_history PROPERTY SOVERSION "0.0")

add_subdirectory (dirhistory)
