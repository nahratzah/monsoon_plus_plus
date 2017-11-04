project (monsoon_engine)

include_directories (include)
include_directories($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
include_directories($<TARGET_PROPERTY:monsoon_expr,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_engine
  src/context.cc
  src/mutable_context.cc
  src/expression.cc
  src/expressions/operators/unop.cc
  src/expressions/operators/negate.cc
  src/expressions/operators/logical_not.cc
  src/expressions/operators/binop.cc
  src/expressions/operators/plus.cc
  src/expressions/operators/minus.cc
  src/expressions/operators/multiply.cc
  src/expressions/operators/divide.cc
  src/expressions/operators/modulo.cc
  src/expressions/operators/logical_and.cc
  src/expressions/operators/logical_or.cc
  src/expressions/operators/equality.cc
  src/expressions/operators/inequality.cc
  src/expressions/operators/cmp_lt.cc
  src/expressions/operators/cmp_gt.cc
  src/expressions/operators/cmp_le.cc
  src/expressions/operators/cmp_ge.cc
  src/expressions/constants/constant.cc
  src/match_clause.cc
  src/match_clauses/default_match_clause.cc
  src/match_clauses/by_clause.cc
  src/match_clauses/without_clause.cc
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
  include/monsoon/expression.h
  include/monsoon/match_clause-inl.h
  include/monsoon/match_clause.h
  include/monsoon/mutable_context.h
  include/monsoon/rule.h
  DESTINATION include/monsoon)
install (FILES
  include/monsoon/expressions/constants/constant-inl.h
  include/monsoon/expressions/constants/constant.h
  DESTINATION include/monsoon/expressions/constants)
install (FILES
  include/monsoon/expressions/operators/binop.h
  include/monsoon/expressions/operators/cmp_ge.h
  include/monsoon/expressions/operators/cmp_gt.h
  include/monsoon/expressions/operators/cmp_le.h
  include/monsoon/expressions/operators/cmp_lt.h
  include/monsoon/expressions/operators/divide.h
  include/monsoon/expressions/operators/equality.h
  include/monsoon/expressions/operators/inequality.h
  include/monsoon/expressions/operators/logical_and.h
  include/monsoon/expressions/operators/logical_not.h
  include/monsoon/expressions/operators/logical_or.h
  include/monsoon/expressions/operators/minus.h
  include/monsoon/expressions/operators/modulo.h
  include/monsoon/expressions/operators/multiply.h
  include/monsoon/expressions/operators/negate.h
  include/monsoon/expressions/operators/plus.h
  include/monsoon/expressions/operators/unop.h
  DESTINATION include/monsoon/expressions/operators)
install (FILES
  include/monsoon/match_clauses/by_clause-inl.h
  include/monsoon/match_clauses/by_clause.h
  include/monsoon/match_clauses/default_match_clause-inl.h
  include/monsoon/match_clauses/default_match_clause.h
  include/monsoon/match_clauses/without_clause-inl.h
  include/monsoon/match_clauses/without_clause.h
  DESTINATION include/monsoon/match_clauses)
install (FILES
  include/monsoon/spirit/expression-inl.h
  include/monsoon/spirit/expression.h
  include/monsoon/spirit/tokenizer-inl.h
  include/monsoon/spirit/tokenizer.h
  DESTINATION include/monsoon/spirit)
