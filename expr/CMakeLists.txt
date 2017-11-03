project (monsoon_expr)

include_directories (include)
include_directories($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_expr
  src/expression.cc
  src/expressions/merger.cc
  src/expressions/selector.cc
  src/expressions/constant.cc
  src/expressions/operators.cc
)
target_link_libraries (monsoon_expr ${extra_libs})
target_link_libraries (monsoon_expr monsoon_intf)
set_property (TARGET monsoon_expr PROPERTY VERSION "0.0")
set_property (TARGET monsoon_expr PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_expr DESTINATION lib)
install (FILES
  include/monsoon/expression.h
  include/monsoon/expression-inl.h
  DESTINATION include/monsoon)
install (FILES
  include/monsoon/expressions/selector.h
  include/monsoon/expressions/constant.h
  include/monsoon/expressions/operators.h
  include/monsoon/expressions/merger.h
  include/monsoon/expressions/merger-inl.h
  DESTINATION include/monsoon/expressions)
