project (monsoon_pipe)

include (CheckFunctionExists)

include_directories (include)
include_directories ($<TARGET_PROPERTY:monsoon_lib,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_pipe
  src/pipe.cc)
target_include_directories(monsoon_pipe PUBLIC include)
target_link_libraries (monsoon_pipe monsoon_lib)
set_property (TARGET monsoon_pipe PROPERTY VERSION "0.0")
set_property (TARGET monsoon_pipe PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_pipe DESTINATION lib)
install (FILES
  include/monsoon/queue/queue_export_.h
  include/monsoon/queue/queue.h
  DESTINATION include/monsoon)
