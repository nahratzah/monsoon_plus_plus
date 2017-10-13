project (monsoon_examples)

include_directories($<TARGET_PROPERTY:monsoon_intf,INTERFACE_INCLUDE_DIRECTORIES>)
include_directories($<TARGET_PROPERTY:monsoon_history,INTERFACE_INCLUDE_DIRECTORIES>)
include_directories($<TARGET_PROPERTY:monsoon_dirhistory,INTERFACE_INCLUDE_DIRECTORIES>)

add_executable (count count.cc)
target_link_libraries (count monsoon_intf monsoon_history monsoon_dirhistory)
target_link_libraries (count ${extra_libs})
