project (monsoon_lib)

include_directories (include)
add_library (monsoon_lib
  src/empty_file.cc
)
target_include_directories(monsoon_lib PUBLIC include)
set_property (TARGET monsoon_lib PROPERTY VERSION "0.0")
set_property (TARGET monsoon_lib PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_lib DESTINATION lib)
install (FILES
  include/monsoon/lib_export_.h
  include/monsoon/hash_support.h
  DESTINATION include/monsoon)

add_subdirectory (misc)
add_subdirectory (queue)
