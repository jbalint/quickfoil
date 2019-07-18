include(CheckCXXSourceCompiles)

CHECK_CXX_SOURCE_COMPILES("
  #include <cstdlib>

  int main() {
    double* value = aligned_alloc(64, sizeof(double));
    free(value);
    return value == nullptr;
  }"
  HAVE_C11_ALIGNED_ALLOC)

CHECK_CXX_SOURCE_COMPILES("
  #include <cstdlib>

  int main() {
    void* value;
    int errno = posix_memalign(&value, 64, sizeof(double));
    free(value);
    return errno;
  }"
  HAVE_POSIX_MEMALIGN)

if (NOT HAVE_C11_ALIGNED_ALLOC AND NOT HAVE_POSIX_MEMALIGN)
  message(FATAL_ERROR "No aligned memory allocation method is found")
endif()

cmake_host_system_information(RESULT TOTAL_PHYSICAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/MemoryConfig.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/MemoryConfig.hpp"
)

add_library(quickfoil_memory_Arena Arena.cpp Arena.hpp)
add_library(quickfoil_memory_Buffer ../empty_src.cpp Buffer.hpp)
add_library(quickfoil_memory_MemoryUsage MemoryUsage.cpp MemoryUsage.hpp)
add_library(quickfoil_memory_MemUtil ../empty_src.cpp MemUtil.hpp)

target_link_libraries(quickfoil_memory_Arena
                      folly
                      quickfoil_utility_Macros
                      quickfoil_utility_Vector)
if(USE_JEMALLOC)
  target_link_libraries(quickfoil_memory_Arena 
                        ${JEMALLOC_LIBRARIES})
endif()
target_link_libraries(quickfoil_memory_Buffer
                      glog
                      quickfoil_memory_MemUtil
                      quickfoil_utility_Macros
                      quickfoil_utility_Vector)
if(USE_JEMALLOC)
  target_link_libraries(quickfoil_memory_Buffer 
                        ${JEMALLOC_LIBRARIES})
endif()
target_link_libraries(quickfoil_memory_MemoryUsage
                      gflags_nothreads-static)

add_executable(quickfoil_memory_Arena_test
               Arena_test.cpp)
target_link_libraries(quickfoil_memory_Arena_test
                      gtest
                      gtest_main
                      quickfoil_memory_Arena)

add_test(quickfoil_memory_Arena_test quickfoil_memory_Arena_test)
