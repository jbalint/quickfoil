# This file is stolen from
# https://github.com/hhvm/hhvm-third-party/blob/master/folly/CMakeLists.txt.

set(FOLLY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/folly")

function(auto_sources RETURN_VALUE PATTERN SOURCE_SUBDIRS)

  if ("${SOURCE_SUBDIRS}" STREQUAL "RECURSE")
    SET(PATH ".")
    if (${ARGC} EQUAL 4)
      list(GET ARGV 3 PATH)
    endif ()
  endif()

  if ("${SOURCE_SUBDIRS}" STREQUAL "RECURSE")
    unset(${RETURN_VALUE})
    file(GLOB SUBDIR_FILES "${PATH}/${PATTERN}")
    list(APPEND ${RETURN_VALUE} ${SUBDIR_FILES})

    file(GLOB subdirs RELATIVE ${PATH} ${PATH}/*)

    foreach(DIR ${subdirs})
      if (IS_DIRECTORY ${PATH}/${DIR})
        if (NOT "${DIR}" STREQUAL "CMakeFiles")
          file(GLOB_RECURSE SUBDIR_FILES "${PATH}/${DIR}/${PATTERN}")
          list(APPEND ${RETURN_VALUE} ${SUBDIR_FILES})
        endif()
      endif()
    endforeach()
  else ()
    file(GLOB ${RETURN_VALUE} "${PATTERN}")

    foreach (PATH ${SOURCE_SUBDIRS})
      file(GLOB SUBDIR_FILES "${PATH}/${PATTERN}")
      list(APPEND ${RETURN_VALUE} ${SUBDIR_FILES})
    endforeach(PATH ${SOURCE_SUBDIRS})
  endif ()

  if (${FILTER_OUT})
    list(REMOVE_ITEM ${RETURN_VALUE} ${FILTER_OUT})
  endif()

  set(${RETURN_VALUE} ${${RETURN_VALUE}} PARENT_SCOPE)
endfunction(auto_sources)

if (NOT EXISTS "${FOLLY_DIR}/Portability.h")
  message(FATAL_ERROR "${FOLLY_DIR}/Portability.h missing, did you forget to "
                      "run `git submodule update --init --recursive`?")
endif()

add_definitions(-DNO_LIB_GFLAGS)

execute_process(COMMAND python2 "${FOLLY_DIR}/build/generate_escape_tables.py" "--install_dir=${CMAKE_CURRENT_BINARY_DIR}") 
execute_process(COMMAND python2 "${FOLLY_DIR}/build/generate_format_tables.py" "--install_dir=${CMAKE_CURRENT_BINARY_DIR}") 
execute_process(COMMAND python2 "${FOLLY_DIR}/build/generate_varint_tables.py" "--install_dir=${CMAKE_CURRENT_BINARY_DIR}") 

# Generated files from folly/build/generate_*.py
auto_sources(genfiles "*.cpp" "RECURSE" "${CMAKE_CURRENT_BINARY_DIR}")

# Main folly library files
auto_sources(files "*.cpp" "RECURSE" "${FOLLY_DIR}")
auto_sources(cfiles "*.c" "RECURSE" "${FOLLY_DIR}")

# No need for tests, Benchmarks, Utils, or most experimental stuff
foreach (file ${files})
  if (${file} MATCHES "/test/")
    list(REMOVE_ITEM files ${file})
  endif()
endforeach()

list(REMOVE_ITEM files
  ${FOLLY_DIR}/Benchmark.cpp
  ${FOLLY_DIR}/build/GenerateFingerprintTables.cpp
  ${FOLLY_DIR}/detail/Clock.cpp
  ${FOLLY_DIR}/detail/Futex.cpp
  ${FOLLY_DIR}/experimental/File.cpp
  ${FOLLY_DIR}/experimental/exception_tracer/ExceptionTracer.cpp
  ${FOLLY_DIR}/experimental/exception_tracer/ExceptionTracerBenchmark.cpp
  ${FOLLY_DIR}/experimental/exception_tracer/ExceptionTracerLib.cpp
  ${FOLLY_DIR}/experimental/exception_tracer/ExceptionTracerTest.cpp
  ${FOLLY_DIR}/experimental/exception_tracer/StackTrace.cpp
  ${FOLLY_DIR}/experimental/io/AsyncIO.cpp
  ${FOLLY_DIR}/experimental/io/HugePageUtil.cpp
  ${FOLLY_DIR}/experimental/io/IOBufQueue.cpp
  ${FOLLY_DIR}/experimental/symbolizer/StackTrace.cpp
  ${FOLLY_DIR}/experimental/symbolizer/ElfCache.cpp
  ${FOLLY_DIR}/experimental/symbolizer/ElfUtil.cpp
  ${FOLLY_DIR}/experimental/symbolizer/SignalHandler.cpp
  )

foreach (file ${files})
  if (${file} MATCHES "/experimental/")
    list(REMOVE_ITEM files ${file})
  elseif (${file} MATCHES "/io/")
    list(REMOVE_ITEM files ${file})
  endif()
endforeach()

# Remove non-portable items
if (NOT LINUX)
  list(REMOVE_ITEM files
    ${FOLLY_DIR}/experimental/symbolizer/Symbolizer.cpp
    ${FOLLY_DIR}/experimental/symbolizer/Dwarf.cpp
    ${FOLLY_DIR}/experimental/symbolizer/Elf.cpp
    )
endif()

find_package(LibEvent REQUIRED)

# Subprocess requires flatmap from boost 1.48
# and some other folly pieces we're not including yet
# For now, that's not actually a requirement, so skip it
list(REMOVE_ITEM files ${FOLLY_DIR}/Subprocess.cpp)

# io/Compression requires snappy library
# Don't add dep until we need it
list(REMOVE_ITEM files ${FOLLY_DIR}/io/Compression.cpp)

include_directories(${Boost_INCLUDE_DIRS})
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/")
include_directories("${PROJECT_BINARY_DIR}/third_party/double-conversion/src/include")
include_directories(${LIBEVENT_INCLUDE_DIR})

find_package(Threads)

if(APPLE)
  set(FOLLY_HAVE_WEAK_SYMBOLS 1)
else()
  # check for weak symbols
  CHECK_CXX_SOURCE_COMPILES("
      extern \"C\" void configure_link_extern_weak_test()
__attribute__((weak));
      int main(int argc, char** argv) {
          return configure_link_extern_weak_test == nullptr;
      }
  "
      FOLLY_HAVE_WEAK_SYMBOLS
  )
endif()

if(FOLLY_HAVE_WEAK_SYMBOLS)
  message(STATUS "Set FOLLY_HAVE_WEAK_SYMBOLS to 1")
  add_definitions(-DFOLLY_HAVE_WEAK_SYMBOLS=1)
else()
  message(STATUS "Set FOLLY_HAVE_WEAK_SYMBOLS to 0")
  add_definitions(-DFOLLY_HAVE_WEAK_SYMBOLS=0)
endif()

# Folly requires msse4.2.
if(NOT HAVE_SSE4_2_EXTENSIONS)
  message(FATAL_ERROR "SSE4.2 is required to build Folly.")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")

add_library(folly STATIC ${files} ${genfiles} ${cfiles} )

target_link_libraries(folly
                      ${Boost_LIBRARIES}
                      double-conversion
                      glog
                      gflags-static
                      ${LIBEVENT_LIBRARY})

if (USE_JEMALLOC)
  message(STATUS "Building folly with jemalloc ${JEMALLOC_LIBRARIES}")
  target_link_libraries(folly ${JEMALLOC_LIBRARIES})
endif()

# For some reason we aren't making a folly-config.h and this is in there.
# Please fix properly!
add_definitions("-DFOLLY_VERSION=\"0.1\"")
