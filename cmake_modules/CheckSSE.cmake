macro(FindSSEFlags)
  include(CheckCXXSourceRuns)
  set(SSE_FLAGS)
 
  set(CMAKE_REQUIRED_FLAGS "-mavx")
  CHECK_CXX_SOURCE_RUNS("
    #include <immintrin.h>
    int main () {
      __m256 a = _mm256_set1_ps(0.5);
      a = _mm256_add_ps(a, a);
      return _mm256_movemask_ps(a);
    }"
    HAVE_AVX_EXTENSIONS)
  
  set(CMAKE_REQUIRED_FLAGS "-msse4.2")
  CHECK_CXX_SOURCE_RUNS("
    #include <smmintrin.h>
    int main () {
      long long a[2] = {1, 2};
      long long b[2];
      __m128i va = _mm_loadu_si128((__m128i*)a);
      _mm_storeu_si128((__m128i*)b, va);
      if (b[0] == 1LL && b[1] == 2LL) {
        return 0;
      }
      return 1;
    }"
    HAVE_SSE4_2_EXTENSIONS)
    
  set(CMAKE_REQUIRED_FLAGS "-msse4.1")
  CHECK_CXX_SOURCE_RUNS("
    #include <smmintrin.h>
    int main () {
      __m128 a = _mm_set1_ps(0.5);
      a = _mm_dp_ps(a, a, 0x77);
      return _mm_movemask_ps(a);
    }"
    HAVE_SSE4_1_EXTENSIONS)
  unset(CMAKE_REQUIRED_FLAGS)
endmacro(FindSSEFlags)

FindSSEFlags()
