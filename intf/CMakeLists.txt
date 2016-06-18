project (monsoon_intf)

include_directories (include)
include_directories($<TARGET_PROPERTY:monsoon_lib,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_intf SHARED
  src/collector.cc
  src/group_name.cc
  src/metric_name.cc
  src/metric_value.cc
  src/simple_group.cc
  src/tags.cc
  src/time_series_value.cc
  src/push_processor.cc
  src/alert.cc
  src/histogram.cc
)
target_link_libraries (monsoon_intf ${extra_libs})
target_link_libraries (monsoon_intf monsoon_lib)
target_include_directories (monsoon_intf PUBLIC $<TARGET_PROPERTY:monsoon_lib,INTERFACE_INCLUDE_DIRECTORIES> include)
set_property (TARGET monsoon_intf PROPERTY VERSION "0.0")
set_property (TARGET monsoon_intf PROPERTY SOVERSION "0.0")
