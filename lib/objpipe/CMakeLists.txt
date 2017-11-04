project (monsoon_objpipe)

include (CheckFunctionExists)

include_directories (include)
include_directories ($<TARGET_PROPERTY:monsoon_lib,INTERFACE_INCLUDE_DIRECTORIES>)
add_library (monsoon_objpipe
  src/base_objpipe.cc
  src/errc.cc)
target_include_directories(monsoon_objpipe PUBLIC include)
target_link_libraries (monsoon_objpipe monsoon_lib)
set_property (TARGET monsoon_objpipe PROPERTY VERSION "0.0")
set_property (TARGET monsoon_objpipe PROPERTY SOVERSION "0.0")

install (TARGETS monsoon_objpipe DESTINATION lib)
install (FILES
  include/monsoon/objpipe/callback.h
  include/monsoon/objpipe/doc_.h
  include/monsoon/objpipe/errc.h
  include/monsoon/objpipe/interlock.h
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
  include/monsoon/objpipe/detail/map_operation.h
  include/monsoon/objpipe/detail/reader_intf.h
  include/monsoon/objpipe/detail/writer_intf.h
  DESTINATION include/monsoon/objpipe/detail)
