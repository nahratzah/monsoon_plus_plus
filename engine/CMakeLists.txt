project (monsoon_engine)

include_directories (include)
include_directories($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_engine SHARED
  src/context.cc
  src/expression.cc
  src/expression/operators/binop.cc
)
target_link_libraries (monsoon_engine ${extra_libs})
target_link_libraries (monsoon_engine monsoon_intf)
set_property (TARGET monsoon_engine PROPERTY VERSION "0.0")
set_property (TARGET monsoon_engine PROPERTY SOVERSION "0.0")
