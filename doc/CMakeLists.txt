find_package(Doxygen REQUIRED mscgen OPTIONAL_COMPONENTS dot)

if(DOXYGEN_FOUND)
  if (DOXYGEN_DOT_EXECUTABLE)
    set(DOXYGEN_HAVE_DOT "YES")
  else ()
    set(DOXYGEN_HAVE_DOT "NO")
  endif ()
  mark_as_advanced(DOXYGEN_HAVE_DOT)

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(doc ALL
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM)
endif(DOXYGEN_FOUND)
