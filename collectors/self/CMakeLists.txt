project (monsoon_collectors_self)

include_directories (include)
include_directories($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_collectors_self
  src/self.cc
)
target_link_libraries (monsoon_collectors_self ${extra_libs})
target_link_libraries (monsoon_collectors_self monsoon_lib)
target_link_libraries (monsoon_collectors_self monsoon_intf)
target_link_libraries (monsoon_collectors_self objpipe)
target_include_directories (monsoon_collectors_self PUBLIC include)
target_include_directories (monsoon_collectors_self PUBLIC $<TARGET_PROPERTY:monsoon_lib,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories (monsoon_collectors_self PUBLIC $<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories (monsoon_collectors_self PUBLIC $<TARGET_PROPERTY:objpipe,INTERFACE_INCLUDE_DIRECTORIES>)
set_property (TARGET monsoon_collectors_self PROPERTY VERSION "0.0")
set_property (TARGET monsoon_collectors_self PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_collectors_self DESTINATION lib)
install (FILES
  include/monsoon/collectors/self.h
  DESTINATION include/monsoon/collectors)

# add_subdirectory (tests)
