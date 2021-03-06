project (monsoon_intf)

find_package (Boost COMPONENTS date_time REQUIRED)
add_library (monsoon_intf
  src/collector.cc
  src/group_name.cc
  src/metric_name.cc
  src/metric_value.cc
  src/simple_group.cc
  src/metric_source.cc
  src/tags.cc
  src/time_point.cc
  src/time_range.cc
  src/time_series_value.cc
  src/time_series.cc
  src/alert.cc
  src/interpolate.cc
  src/histogram.cc
  src/config_support.cc
  src/path_matcher.cc
  src/tag_matcher.cc
  src/path_common.cc
  src/instrumentation.cc
  src/grammar/intf/ast.cc
  src/grammar/intf/rules.cc
)
target_link_libraries (monsoon_intf PUBLIC monsoon_lib)
target_link_libraries (monsoon_intf PUBLIC objpipe)
target_link_libraries (monsoon_intf PUBLIC monsoon_cache)
target_link_libraries (monsoon_intf PUBLIC instrumentation)
target_link_libraries (monsoon_intf PRIVATE ${Boost_DATE_TIME_LIBRARY})
target_include_directories (monsoon_intf PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
set_property (TARGET monsoon_intf PROPERTY VERSION "0.0")
set_property (TARGET monsoon_intf PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_intf DESTINATION lib)
install (FILES
  include/monsoon/intf_export_.h
  include/monsoon/interpolate.h
  include/monsoon/metric_source.h
  include/monsoon/alert-inl.h
  include/monsoon/alert.h
  include/monsoon/collector.h
  include/monsoon/config_support.h
  include/monsoon/group_name-inl.h
  include/monsoon/group_name.h
  include/monsoon/histogram-inl.h
  include/monsoon/histogram.h
  include/monsoon/metric_name-inl.h
  include/monsoon/metric_name.h
  include/monsoon/metric_value-inl.h
  include/monsoon/metric_value.h
  include/monsoon/simple_group-inl.h
  include/monsoon/simple_group.h
  include/monsoon/tags-inl.h
  include/monsoon/tags.h
  include/monsoon/time_point-inl.h
  include/monsoon/time_point.h
  include/monsoon/time_range-inl.h
  include/monsoon/time_range.h
  include/monsoon/time_series-inl.h
  include/monsoon/time_series.h
  include/monsoon/time_series_value-inl.h
  include/monsoon/time_series_value.h
  include/monsoon/path_matcher.h
  include/monsoon/path_matcher-inl.h
  include/monsoon/tag_matcher.h
  include/monsoon/tag_matcher-inl.h
  include/monsoon/path_common.h
  include/monsoon/path_common-inl.h
  DESTINATION include/monsoon)
install (FILES
  include/monsoon/grammar/intf/ast.h
  include/monsoon/grammar/intf/ast_adapted.h
  include/monsoon/grammar/intf/rules.h
  DESTINATION include/monsoon/grammar/intf)

add_subdirectory(tests)
