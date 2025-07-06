// cmake/test_simd.cpp
#include <immintrin.h>
#include <cstdlib>

int main() {
    try {
        volatile int result = 0;

        #ifdef TEST_SSE
            __m128i sse_vec = _mm_setzero_si128();
            result = _mm_extract_epi16(sse_vec, 0);
        #endif

        #ifdef TEST_SSE2
            __m128i sse2_vec = _mm_setzero_si128();
            result = _mm_extract_epi16(sse2_vec, 0);
        #endif

        #ifdef TEST_SSE4_1
            __m128i sse41_vec = _mm_setzero_si128();
            result = _mm_extract_epi32(sse41_vec, 0);
        #endif

        #ifdef TEST_SSE4_2
            __m128i sse42_vec = _mm_setzero_si128();
            result = _mm_crc32_u32(0, 42);
        #endif

        #ifdef TEST_AVX
            __m256i avx_vec = _mm256_setzero_si256();
            result = _mm256_extract_epi32(avx_vec, 0);
        #endif

        #ifdef TEST_AVX2
            __m256i avx2_vec = _mm256_setzero_si256();
            __m256i avx2_add = _mm256_add_epi32(avx2_vec, avx2_vec);
            result = _mm256_extract_epi32(avx2_add, 0);
        #endif

        #ifdef TEST_BMI
            result = __builtin_popcount(42);
        #endif

        #ifdef TEST_AVX_512
            __m512i avx512_vec = _mm512_setzero_si512();
            result = _mm512_extract_epi32(avx512_vec, 0);
        #endif

        return result;
    } catch (...) {
        return 1;
    }
}