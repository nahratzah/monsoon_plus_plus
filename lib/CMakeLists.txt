project (monsoon_lib)

include_directories (include)
add_library (monsoon_lib SHARED
  src/optional.cc
  src/any.cc
)
target_include_directories(monsoon_lib PUBLIC include)
set_property (TARGET monsoon_lib PROPERTY VERSION "0.0")
set_property (TARGET monsoon_lib PROPERTY SOVERSION "0.0")
