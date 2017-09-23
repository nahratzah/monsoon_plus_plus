include(CTest)
find_package(UnitTest++)

if (UnitTest++_FOUND)
  include_directories(${UTPP_INCLUDE_DIRS})

  add_executable (test_tsdata tsdata.cc)
  target_link_libraries (test_tsdata monsoon_dirhistory)
  target_link_libraries (test_tsdata UnitTest++)
  add_test (tsdata test_tsdata)
endif ()
