project (monsoon_collectors_self)

add_library (monsoon_collectors_self
  src/self.cc
)
target_link_libraries (monsoon_collectors_self PUBLIC monsoon_intf)
target_link_libraries (monsoon_collectors_self PUBLIC objpipe)
target_link_libraries (monsoon_collectors_self PUBLIC instrumentation)
target_include_directories (monsoon_collectors_self PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
set_property (TARGET monsoon_collectors_self PROPERTY VERSION "0.0")
set_property (TARGET monsoon_collectors_self PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_collectors_self DESTINATION lib)
install (FILES
  include/monsoon/collectors/self.h
  DESTINATION include/monsoon/collectors)

# add_subdirectory (tests)
