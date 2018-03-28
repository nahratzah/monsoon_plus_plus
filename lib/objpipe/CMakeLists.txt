project (monsoon_objpipe)

find_package (Boost COMPONENTS context REQUIRED)

add_library (monsoon_objpipe INTERFACE)
target_include_directories(monsoon_objpipe INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${Boost_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:${Boost_INCLUDE_DIR}>)
target_link_libraries (monsoon_objpipe INTERFACE ${Boost_CONTEXT_LIBRARY})

find_package (Threads REQUIRED)
if(CMAKE_USE_PTHREADS_INIT)
  target_link_libraries(monsoon_objpipe INTERFACE ${CMAKE_THREAD_LIBS_INIT})
endif()

install (FILES
  include/monsoon/objpipe/callback.h
  include/monsoon/objpipe/doc_.h
  include/monsoon/objpipe/errc.h
  include/monsoon/objpipe/interlock.h
  include/monsoon/objpipe/array.h
  include/monsoon/objpipe/objpipe_export_.h
  include/monsoon/objpipe/pull_reader.h
  include/monsoon/objpipe/reader.h
  include/monsoon/objpipe/writer.h
  DESTINATION include/monsoon/objpipe)
install (FILES
  include/monsoon/objpipe/detail/base_objpipe.h
  include/monsoon/objpipe/detail/callbacked.h
  include/monsoon/objpipe/detail/filter_operation.h
  include/monsoon/objpipe/detail/interlocked.h
  include/monsoon/objpipe/detail/arrayed.h
  include/monsoon/objpipe/detail/map_operation.h
  include/monsoon/objpipe/detail/reader_intf.h
  include/monsoon/objpipe/detail/writer_intf.h
  DESTINATION include/monsoon/objpipe/detail)

add_subdirectory (tests)
