include(CTest)
find_package(UnitTest++)

if (UnitTest++_FOUND)
  include_directories(${UTPP_INCLUDE_DIRS})

  add_executable (test_tsdata tsdata.cc tsdata_cpp.cc)
  target_link_libraries (test_tsdata monsoon_dirhistory)
  target_link_libraries (test_tsdata UnitTest++)
  target_link_libraries (test_tsdata ${extra_libs})
  add_test (tsdata test_tsdata "${CMAKE_CURRENT_SOURCE_DIR}")
endif ()

add_executable (count count.cc)
target_link_libraries (count monsoon_dirhistory)
target_link_libraries (count ${extra_libs})
