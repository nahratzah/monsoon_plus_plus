project (monsoon_queue)

include (CheckFunctionExists)

include_directories (include)
include_directories ($<TARGET_PROPERTY:monsoon_lib,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_queue
  src/queue.cc)
target_include_directories(monsoon_queue PUBLIC include)
target_link_libraries (monsoon_queue monsoon_lib)
set_property (TARGET monsoon_queue PROPERTY VERSION "0.0")
set_property (TARGET monsoon_queue PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_queue DESTINATION lib)
install (FILES
  include/monsoon/queue/queue_export_.h
  include/monsoon/queue/queue.h
  DESTINATION include/monsoon)
