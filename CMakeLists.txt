cmake_minimum_required (VERSION 3.3)
project (monsoon)
include (CTest)

include (CheckCXXCompilerFlag)

enable_testing()
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_EXAMPLES "Build example binaries" ON)

# If on apple, use clang and libc++.
if (APPLE)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvm.clang.1_0")
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++14")
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
  set (CMAKE_OSX_ARCHITECTURES "i386;x86_64")
endif ()

# Set up the compiler.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3")
  set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
  set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wno-missing-noreturn -Wno-unused -Wsign-compare -Wuninitialized -Wno-unused-value -Wno-unknown-attributes")

  # Linux glibc is a pain without this.
  # This is the shotgun approach: enable every standard and make functions thread-safe.
  add_definitions (-D_REENTRANT -D_GNU_SOURCE)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
  set (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
  set (CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG")
  set (CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} /DEBUG")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /MP")
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE /NXCOMPAT /INCREMENTAL:NO")
  set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LARGEADDRESSAWARE /NXCOMPAT /INCREMENTAL:NO")
  set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /LARGEADDRESSAWARE /NXCOMPAT /INCREMENTAL:NO")
endif ()

# If using ninja and clang, enable colours.
if (UNIX AND
    CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
    CMAKE_GENERATOR STREQUAL "Ninja")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
endif()

# Enable gprof profiling.
option (PROFILE "Enable profiling." OFF)
if (PROFILE)
  check_cxx_compiler_flag ("-pg" PROFILE_PG)
  if (PROFILE_PG)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER} -pg")
    set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -pg")
  else ()
    message (ERROR "Cannot profile.")
  endif ()
endif ()

# Enable C++14 and C++17 using flags, if the compiler has them.
check_cxx_compiler_flag ("-std=c++17" STD_CXX17)
check_cxx_compiler_flag ("-std=c++1z" STD_CXX1Z)
check_cxx_compiler_flag ("-std=c++14" STD_CXX14)
if (STD_CXX17)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
elseif (STD_CXX1Z)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")
elseif (STD_CXX14)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
endif ()

# This library uses threads.
# Link against the thread-lib.
find_package (Threads REQUIRED)
if (CMAKE_USE_PTHREADS_INIT)
  # XXX is this the correct way for all OSes?
  list (APPEND CMAKE_REQUIRED_LIBRARIES "${CMAKE_THREAD_LIBS_INIT}")
endif ()

find_package(instrumentation 0.0)
if(NOT instrumentation_DIR)
  add_subdirectory(contrib/instrumentation)
endif()

find_package(cycle_ptr 0.0)
if(NOT cycle_ptr_DIR)
  add_subdirectory(contrib/cycle_ptr)
endif()

find_package(monsoon_cache 0.2)
if(NOT monsoon_cache_DIR)
  add_subdirectory(contrib/monsoon_cache)
endif()

find_package(objpipe)
if(NOT objpipe_DIR)
  add_subdirectory(contrib/objpipe)
endif()

# Add module directories.
add_subdirectory (bin)
add_subdirectory (lib)
add_subdirectory (intf)
add_subdirectory (engine)
add_subdirectory (history)
add_subdirectory (expr)
add_subdirectory (collectors)

if (BUILD_EXAMPLES)
  add_subdirectory (examples)
endif()

add_subdirectory (doc)
