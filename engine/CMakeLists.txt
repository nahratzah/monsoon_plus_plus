project (monsoon_engine)

include_directories (include)
include_directories($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_engine SHARED
  src/context.cc
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
  src/match_clause.cc
  src/match_clauses/default_match_clause.cc
  src/match_clauses/by_clause.cc
  src/match_clauses/without_clause.cc
)
target_link_libraries (monsoon_engine ${extra_libs})
target_link_libraries (monsoon_engine monsoon_intf)
set_property (TARGET monsoon_engine PROPERTY VERSION "0.0")
set_property (TARGET monsoon_engine PROPERTY SOVERSION "0.0")
