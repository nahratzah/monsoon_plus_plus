project (monsoon_lib)

find_path (STD_MEMORY_RESOURCE NAMES memory_resource)
mark_as_advanced (STD_MEMORY_RESOURCE)
if (NOT STD_MEMORY_RESOURCE)
  find_package (Boost COMPONENTS container REQUIRED)
endif ()

include_directories (include)
add_library (monsoon_lib INTERFACE)
target_include_directories(monsoon_lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    )

if (NOT STD_MEMORY_RESOURCE)
  target_link_libraries(monsoon_lib INTERFACE ${Boost_CONTAINER_LIBRARY})
  target_include_directories(monsoon_lib INTERFACE ${Boost_INCLUDE_DIR})
endif ()

install (TARGETS monsoon_lib DESTINATION lib)
install (FILES
  include/monsoon/lib_export_.h
  include/monsoon/hash_support.h
  include/monsoon/overload.h
  include/monsoon/memoid.h
  include/monsoon/cheap_fn_ref.h
  include/monsoon/unique_alloc_ptr.h
  include/monsoon/shared_resource_allocator.h
  DESTINATION include/monsoon)

add_subdirectory(misc)
add_subdirectory(tx)
add_subdirectory(tests)
