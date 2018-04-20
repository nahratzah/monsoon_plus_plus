project (monsoon_engine)

add_library (monsoon_engine
  src/context.cc
  src/mutable_context.cc
  src/rule.cc
  src/build_task.cc
)
target_include_directories (monsoon_engine PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries (monsoon_engine PUBLIC monsoon_intf)
target_link_libraries (monsoon_engine PUBLIC monsoon_expr)
target_link_libraries (monsoon_engine PUBLIC monsoon_history)
set_property (TARGET monsoon_engine PROPERTY VERSION "0.0")
set_property (TARGET monsoon_engine PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_engine DESTINATION lib)
install (FILES
  include/monsoon/configuration-inl.h
  include/monsoon/configuration.h
  include/monsoon/context.h
  include/monsoon/expr_result-inl.h
  include/monsoon/expr_result.h
  include/monsoon/mutable_context.h
  include/monsoon/rule.h
  include/monsoon/build_task.h
  DESTINATION include/monsoon)
