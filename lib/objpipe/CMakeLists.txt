project (monsoon_objpipe)

include (CheckFunctionExists)

include_directories (include)
include_directories ($<TARGET_PROPERTY:monsoon_lib,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_objpipe
  src/errc.cc
  src/pipe.cc)
target_include_directories(monsoon_objpipe PUBLIC include)
target_link_libraries (monsoon_objpipe monsoon_lib)
set_property (TARGET monsoon_objpipe PROPERTY VERSION "0.0")
set_property (TARGET monsoon_objpipe PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_objpipe DESTINATION lib)
install (FILES
  include/monsoon/objpipe/queue_export_.h
  include/monsoon/objpipe/pipe.h
  DESTINATION include/monsoon/objpipe)
