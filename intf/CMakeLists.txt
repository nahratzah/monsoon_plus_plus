project (monsoon_intf)

find_package (Boost COMPONENTS date_time REQUIRED)
include_directories (include)
include_directories (${Boost_INCLUDE_DIR})
include_directories($<TARGET_PROPERTY:monsoon_lib,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_intf
  src/collector.cc
  src/group_name.cc
  src/metric_name.cc
  src/metric_value.cc
  src/simple_group.cc
  src/tags.cc
  src/time_point.cc
  src/time_series_value.cc
  src/push_processor.cc
  src/alert.cc
  src/histogram.cc
  src/config_support.cc
)
target_link_libraries (monsoon_intf ${extra_libs})
target_link_libraries (monsoon_intf monsoon_lib)
target_link_libraries (monsoon_intf ${Boost_DATE_TIME_LIBRARY})
target_include_directories (monsoon_intf PUBLIC $<TARGET_PROPERTY:monsoon_lib,INTERFACE_INCLUDE_DIRECTORIES> include)
set_property (TARGET monsoon_intf PROPERTY VERSION "0.0")
set_property (TARGET monsoon_intf PROPERTY SOVERSION "0.0")
