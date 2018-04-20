project (monsoon_expr)

add_library (monsoon_expr
  src/expression.cc
  src/expressions/merger.cc
  src/expressions/selector.cc
  src/expressions/constant.cc
  src/expressions/operators.cc
  src/match_clause.cc
  src/grammar/expression/ast.cc
  src/grammar/expression/rules.cc
)
target_link_libraries (monsoon_expr PRIVATE monsoon_lib)
target_link_libraries (monsoon_expr PUBLIC monsoon_intf)
target_link_libraries (monsoon_expr PUBLIC objpipe)
target_include_directories (monsoon_expr PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
set_property (TARGET monsoon_expr PROPERTY VERSION "0.0")
set_property (TARGET monsoon_expr PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_expr DESTINATION lib)
install (FILES
  include/monsoon/expression.h
  include/monsoon/expression-inl.h
  include/monsoon/match_clause.h
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
