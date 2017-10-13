include(CTest)
find_package(UnitTest++)

macro (do_test binary result)
  add_executable (test_${binary} ${binary}.cc)
  target_link_libraries (test_${binary} monsoon_misc)
  target_link_libraries (test_${binary} ${extra_libs})
  add_test (${binary} test_${binary})
  set_tests_properties (${binary}
    PROPERTIES PASS_REGULAR_EXPRESSION ${result})
endmacro (do_test)

do_test (file_creation "foobar")
do_test (create_non_existing "monsoon::fd::create threw exception")
do_test (gzip_decompress "chocoladevla")
do_test (gzip_compress "No cats were harmed in the making of this test.")

if (UnitTest++_FOUND)
  include_directories(${UTPP_INCLUDE_DIRS})

  add_executable (test_xdr_encdec xdr_encdec.cc)
  target_link_libraries (test_xdr_encdec monsoon_misc)
  target_link_libraries (test_xdr_encdec UnitTest++)
  target_link_libraries (test_xdr_encdec ${extra_libs})
  add_test (xdr_encdec test_xdr_encdec)
endif ()
