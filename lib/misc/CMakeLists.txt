project (monsoon_misc)

find_package (Boost REQUIRED)
include_directories (include)
include_directories (${Boost_INCLUDE_DIR})
add_library (monsoon_misc SHARED
  src/xdr.cc
)
target_include_directories(monsoon_misc PUBLIC include)
set_property (TARGET monsoon_lib PROPERTY VERSION "0.0")
set_property (TARGET monsoon_lib PROPERTY SOVERSION "0.0")
