project (monsoon_misc)

include (CheckFunctionExists)

find_package (Boost REQUIRED)
find_package (ZLIB REQUIRED)

find_path (STD_FILESYSTEM NAMES filesystem)
mark_as_advanced (STD_FILESYSTEM)
if (NOT STD_FILESYSTEM)
  find_package (Boost COMPONENTS filesystem REQUIRED)
endif ()

add_library (monsoon_misc
  src/fd.cc
  src/stream.cc
  src/ptr_stream.cc
  src/positional_stream.cc
  src/xdr.cc
  src/gzip_stream.cc
  src/aio.cc
)
target_include_directories (monsoon_misc PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_include_directories (monsoon_misc PUBLIC ${Boost_INCLUDE_DIR})
target_include_directories (monsoon_misc PRIVATE ${ZLIB_INCLUDE_DIRS})
if (NOT STD_FILESYSTEM)
  target_link_libraries (monsoon_misc PRIVATE ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY})
endif ()
target_link_libraries (monsoon_misc PRIVATE ${ZLIB_LIBRARIES})
set_property (TARGET monsoon_misc PROPERTY VERSION "0.0")
set_property (TARGET monsoon_misc PROPERTY SOVERSION "0.0")

if (NOT WIN32)
  CHECK_FUNCTION_EXISTS (mkstemp HAS_MKSTEMP)
  mark_as_advanced (HAS_MKSTEMP)
  if (HAS_MKSTEMP)
    target_compile_definitions(monsoon_misc PRIVATE HAS_MKSTEMP)
  else ()
    message (WARNING "no mkstemp() function found")
  endif ()

  find_library (RT_LIB rt)
  if (RT_LIB)
    target_link_libraries (monsoon_misc PRIVATE ${RT_LIB})
  endif ()
endif ()

install (TARGETS monsoon_misc DESTINATION lib)
install (FILES
  include/monsoon/misc_export_.h
  DESTINATION include/monsoon)
install (FILES
  include/monsoon/io/fd.h
  include/monsoon/io/ptr_stream.h
  include/monsoon/io/ptr_stream-inl.h
  include/monsoon/io/gzip_stream.h
  include/monsoon/io/gzip_stream-inl.h
  include/monsoon/io/positional_stream-inl.h
  include/monsoon/io/positional_stream.h
  include/monsoon/io/limited_stream.h
  include/monsoon/io/stream.h
  include/monsoon/io/aio.h
  DESTINATION include/monsoon/io)
install (FILES
  include/monsoon/xdr/xdr-inl.h
  include/monsoon/xdr/xdr.h
  include/monsoon/xdr/xdr_stream-inl.h
  include/monsoon/xdr/xdr_stream.h
  DESTINATION include/monsoon/xdr)

add_subdirectory (tests)
