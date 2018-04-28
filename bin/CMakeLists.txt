add_executable (monsoond
  src/monsoond.cc)

target_link_libraries (monsoond PUBLIC monsoon_intf)
target_link_libraries (monsoond PUBLIC monsoon_engine)
target_link_libraries (monsoond PUBLIC monsoon_history)
target_link_libraries (monsoond PUBLIC monsoon_collectors_self)

install (TARGETS monsoond DESTINATION sbin)
