project (monsoon_expr)

include_directories (include)
include_directories($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_expr
  src/expression.cc
  src/expressions/merger.cc
  src/expressions/selector.cc
  src/expressions/constant.cc
  src/expressions/operators.cc
  src/path_matcher.cc
  src/match_clause.cc
  src/grammar/expression/ast.cc
)
target_link_libraries (monsoon_expr ${extra_libs})
target_link_libraries (monsoon_expr monsoon_lib)
target_link_libraries (monsoon_expr monsoon_intf)
target_link_libraries (monsoon_expr monsoon_objpipe)
target_include_directories (monsoon_expr PUBLIC include)
target_include_directories (monsoon_expr PUBLIC $<TARGET_PROPERTY:monsoon_lib,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories (monsoon_expr PUBLIC $<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories (monsoon_expr PUBLIC $<TARGET_PROPERTY:monsoon_objpipe,INTERFACE_INCLUDE_DIRECTORIES>)
set_property (TARGET monsoon_expr PROPERTY VERSION "0.0")
set_property (TARGET monsoon_expr PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_expr DESTINATION lib)
install (FILES
  include/monsoon/expression.h
  include/monsoon/expression-inl.h
  include/monsoon/match_clause.h
  include/monsoon/path_matcher.h
  include/monsoon/path_matcher-inl.h
  DESTINATION include/monsoon)
install (FILES
  include/monsoon/expressions/selector.h
  include/monsoon/expressions/constant.h
  include/monsoon/expressions/operators.h
  include/monsoon/expressions/merger.h
  include/monsoon/expressions/merger-inl.h
  DESTINATION include/monsoon/expressions)
install (FILES
  include/monsoon/grammar/expression/ast.h
  include/monsoon/grammar/expression/ast_adapted.h
  include/monsoon/grammar/expression/rules.h
  DESTINATION include/monsoon/grammar/expression)

add_subdirectory (tests)
