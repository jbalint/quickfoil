#
# This file copyright (c) 2014.
# All rights reserved.
#
cmake_minimum_required (VERSION 3.0.2)

project (QUICKFOIL)

if (WIN32)
  message(FATAL_ERROR "Sorry, Windows is not supported.")
endif()

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules)
include(CheckSSE)

# Check for 64-bit build flags.
set(CMAKE_REQUIRED_LIBRARIES "-m64")
CHECK_C_COMPILER_FLAG("-m64" C_COMPILER_HAS_M64)
CHECK_CXX_COMPILER_FLAG("-m64" CXX_COMPILER_HAS_M64)
unset(CMAKE_REQUIRED_LIBRARIES)

if(C_COMPILER_HAS_M64 AND CXX_COMPILER_HAS_M64)
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -m64")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m64")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -m64")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m64")
endif()

option(USE_JEMALLOC "Use jemalloc instead of system-provided malloc" ON)
if(USE_JEMALLOC)
  find_package(JeMalloc)
  if (NOT JEMALLOC_FOUND)
    set(USE_JEMALLOC OFF)
  else()
    message(STATUS "JeMalloc is used")
    include_directories(${JEMALLOC_INCLUDE_DIR})
  endif()
endif()

option(MONITOR_MEMORY "Keep track of the amount of memory usage by QuickFOIL" OFF)
if(MONITOR_MEMORY)
  add_definitions(-DQUICKFOIL_ENABLE_MEMORY_MONITOR)
endif()

string(TOUPPER "${CMAKE_BUILD_TYPE}" LOWER_CMAKE_BUILD_TYPE)
if (ENABLE_TIMING OR (LOWER_CMAKE_BUILD_TYPE STREQUAL "DEBUG"))
  message(STATUS "Detailed timeing is enabled")
  add_definitions(-DENABLE_TIMING)
else()
  message(STATUS "Detailed timeing is disabled")
endif()

if (ENABLE_LOGGING OR (LOWER_CMAKE_BUILD_TYPE STREQUAL "DEBUG"))
  message(STATUS "Logging is enabled")
  add_definitions(-DENABLE_LOGGING)
else()
  message(STATUS "Logging is disabled")
endif()

# Check for C99 stdint types.
include(CheckTypeSize)
CHECK_TYPE_SIZE("int8_t" INT8_T)
CHECK_TYPE_SIZE("uint8_t" UINT8_T)
CHECK_TYPE_SIZE("int16_t" INT16_T)
CHECK_TYPE_SIZE("uint16_t" UINT16_T)
CHECK_TYPE_SIZE("int32_t" INT32_T)
CHECK_TYPE_SIZE("uint32_t" UINT32_T)
CHECK_TYPE_SIZE("int64_t" INT64_T)
CHECK_TYPE_SIZE("uint64_t" UINT64_T)
if(NOT (HAVE_INT8_T AND HAVE_UINT8_T AND HAVE_INT16_T AND HAVE_UINT16_T
        AND HAVE_INT32_T AND HAVE_UINT32_T AND HAVE_INT64_T AND HAVE_UINT64_T))
  message(FATAL_ERROR "Some C99 stdint types are missing.")
endif()

# Check for C++11 support.
CHECK_CXX_COMPILER_FLAG("-std=c++11" CXX_COMPILER_SUPPORTS_CPP11)
if (CXX_COMPILER_SUPPORTS_CPP11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
  message(FATAL_ERROR "The compiler does not support C++11")
endif()
  
# Generate all warnings for a debug build.
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -pedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -pedantic")

# Strip binaries for release builds.
if(NOT (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" AND APPLE))
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
  # Optimization flags for GCC.

  # If using jemalloc, prevent GCC from making some optimizations on the assumption
  # that it is using its own, built-in malloc.
  if(USE_JEMALLOC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
  endif()
  
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

  # Loop-transformation optimizations that aren't turned on by -O3
  CHECK_CXX_COMPILER_FLAG("-ftree-loop-linear" GCC_HAS_FTREE_LOOP_LINEAR)
  if (GCC_HAS_FTREE_LOOP_LINEAR)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ftree-loop-linear")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ftree-loop-linear")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -ftree-loop-linear")
  endif()

  CHECK_CXX_COMPILER_FLAG("-ftree-loop-distribution" GCC_HAS_FTREE_LOOP_DISTRIBUTION)
  if (GCC_HAS_FTREE_LOOP_DISTRIBUTION)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ftree-loop-distribution")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ftree-loop-distribution")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -ftree-loop-distribution")
  endif()

  CHECK_CXX_COMPILER_FLAG("-floop-strip-mine" GCC_HAS_FLOOP_STRIP_MINE)
  if (GCC_HAS_FLOOP_STRIP_MINE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -floop-strip-mine")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -floop-strip-mine")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -floop-strip-mine")
  endif()

  CHECK_CXX_COMPILER_FLAG("-floop-block" GCC_HAS_FLOOP_BLOCK)
  if (GCC_HAS_FLOOP_BLOCK)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -floop-block")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -floop-block")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -floop-block")
  endif()

  # Enable Link Time Optimization if it is upported.
  CHECK_CXX_COMPILER_FLAG("-flto" GCC_HAS_FLTO)
  set(CMAKE_REQUIRED_FLAGS "-Wl,-fuse-linker-plugin")
  CHECK_CXX_COMPILER_FLAG("" GCC_USE_EXTERNAL_LINKER)
  unset(CMAKE_REQUIRED_FLAGS)
  
  if(GCC_HAS_FLTO)
    set(LTO_FLAGS "-flto")
    if(GCC_USE_EXTERNAL_LINKER)
      set(LTO_FLAGS "${LTO_FLAGS} -fuse-linker-plugin")
    endif()
    execute_process(COMMAND "gcc-ar" ERROR_VARIABLE error_message)
    string(FIND "${error_message}" "no operation" error_position)
    if (NOT error_position EQUAL -1)
      message(STATUS "The gcc wrappers gcc-ar and gcc-ranlib are used")
      set(CMAKE_AR     "gcc-ar")
      set(CMAKE_RANLIB "gcc-ranlib")
    else()
      message(STATUS "gcc-ar and gcc-ranlib are not found. Disable LTO if build is not sucessful.")
    endif()
    message(STATUS
            "Link time optimization is enabled: ${LTO_FLAGS}.")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${LTO_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${LTO_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${LTO_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} ${LTO_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} ${LTO_FLAGS}")
  endif()
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  # Optimization flags for Clang.
  
  # Enable Link Time Optimization if it is upported.
  CHECK_CXX_COMPILER_FLAG("-flto" CLANG_HAS_FLTO)
  if(CLANG_HAS_FLTO)
    message(STATUS "Link time optimization is enabled.")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -flto")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} -flto")
  endif()
  
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

# Enable optimizations based on the host architecture.
CHECK_CXX_COMPILER_FLAG("-march=native" HAS_MARCH_NATIVE)
if (HAS_MARCH_NATIVE AND NOT (CMAKE_COMPILER_IS_GNUCXX AND APPLE))
  set(SSE_FLAGS "${SSE_FLAGS} -march=native")
endif()

if(HAVE_AVX_EXTENSIONS)
  set(SSE_FLAGS "${SSE_FLAGS} -mavx -mfpmath=sse")
elseif(HAVE_SSE4_2_EXTENSIONS)
  set(SSE_FLAGS "${SSE_FLAGS} -msse4.2 -mfpmath=sse")
elseif(HAVE_SSE4_1_EXTENSIONS)
  set(SSE_FLAGS "${SSE_FLAGS} -msse4.1 -mfpmath=sse")
endif()

message(STATUS "Use SSE flags: ${SSE_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${SSE_FLAGS}")

# Set the include directories to the project root directory and the root of the build tree (where
# generated headers will go).
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Setup for third-party libs.
set(THIRD_PARTY_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party")
include(ExternalProject)

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

include(folly_setup)

if (STATIC_BUILD)
  set(Boost_USE_STATIC_LIBS ON)
endif()
find_package(Boost 1.49.0 COMPONENTS system filesystem regex program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_subdirectory("${THIRD_PARTY_SOURCE_DIR}/gtest")
include_directories("${THIRD_PARTY_SOURCE_DIR}/gtest/include")
enable_testing()

add_subdirectory("${THIRD_PARTY_SOURCE_DIR}/gflags")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/third_party/gflags/include")

add_subdirectory("${THIRD_PARTY_SOURCE_DIR}/glog")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/third_party/glog")
include_directories("${THIRD_PARTY_SOURCE_DIR}/glog/src")

include_directories("${PROJECT_BINARY_DIR}/third_party/double-conversion/src/include")

function(ms_underscores_to_camel_case VarIn VarOut)
  string(REPLACE "_" ";" Pieces ${VarIn})
  foreach(Part ${Pieces})
    string(SUBSTRING ${Part} 0 1 Initial)
    string(SUBSTRING ${Part} 1 -1 Part)
    string(TOUPPER ${Initial} Initial)
    set(CamelCase ${CamelCase}${Initial}${Part})
  endforeach()
  set(${VarOut} ${CamelCase} PARENT_SCOPE)
endfunction()

add_subdirectory(third_party/double-conversion)
add_subdirectory(third_party/folly)
include_directories(third_party/boost)

include_directories("${THIRD_PARTY_SOURCE_DIR}/folly/src")

add_subdirectory(expressions)
add_subdirectory(learner)
add_subdirectory(main)
add_subdirectory(memory)
add_subdirectory(operations)
add_subdirectory(schema)
add_subdirectory(storage)
add_subdirectory(types)
add_subdirectory(utility)

add_executable(quickfoil main/main.cpp)
target_link_libraries(quickfoil
                      gflags_nothreads-static
                      glog
                      quickfoil_memory_Buffer
                      quickfoil_learner_QuickFoil
                      quickfoil_learner_QuickFoilTestRunner
                      quickfoil_learner_QuickFoilTimer
                      quickfoil_main_Configuration
                      quickfoil_schema_FoilClause
                      quickfoil_schema_FoilPredicate
                      quickfoil_schema_TypeDefs
                      quickfoil_types_FromString
                      quickfoil_types_TypeID
                      quickfoil_types_TypeTraits
                      quickfoil_utility_ElementDeleter
                      quickfoil_utility_Vector
                      ${BOOST_LIBRARIES})
