project (monsoon_engine)

include_directories (include)
include_directories($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
include_directories($<TARGET_PROPERTY:monsoon_expr,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_engine
  src/context.cc
  src/mutable_context.cc
  src/rule.cc
)
target_link_libraries (monsoon_engine ${extra_libs})
target_link_libraries (monsoon_engine monsoon_intf)
target_link_libraries (monsoon_engine monsoon_expr)
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
  DESTINATION include/monsoon)
