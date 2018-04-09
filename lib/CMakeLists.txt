project (monsoon_lib)

include_directories (include)
add_library (monsoon_lib INTERFACE)
target_include_directories(monsoon_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    )

install (TARGETS monsoon_lib DESTINATION lib)
install (FILES
  include/monsoon/lib_export_.h
  include/monsoon/hash_support.h
  include/monsoon/overload.h
  include/monsoon/memoid.h
  DESTINATION include/monsoon)

add_subdirectory (misc)
add_subdirectory(tests)
