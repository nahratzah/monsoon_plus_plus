cmake_minimum_required (VERSION 2.8)
project (monsoon)
include (CTest)

include (CheckCXXCompilerFlag)

# Declare list variables.
list (APPEND extra_libs)  # Libs against which to link all libraries.

# If on apple, use clang and libc++.
if (APPLE)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvm.clang.1_0")
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++14")
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
  set (CMAKE_OSX_ARCHITECTURES "i386;x86_64")
endif ()

# Set up the compiler.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -fvisibility=hidden")
  set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3")
  set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
  set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wno-missing-noreturn -Wno-unused -Wsign-compare -Wuninitialized -Wno-unused-value")

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
check_cxx_compiler_flag ("-std=c++14" STD_CXX14)
if (STD_CXX17)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
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
list (APPEND extra_libs ${CMAKE_THREAD_LIBS_INIT})
# FreeBSD requires inclusion of librt, linux requires it for clock_gettime.
find_library (RT_LIB rt)
if (RT_LIB)
  list (APPEND extra_libs ${RT_LIB})
endif()


# Add module directories.
add_subdirectory (lib)
add_subdirectory (intf)
add_subdirectory (engine)
add_subdirectory (history)
