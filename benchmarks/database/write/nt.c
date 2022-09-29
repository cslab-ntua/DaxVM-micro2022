#include <immintrin.h>

void _avx_async_cpy(void *d, const void *s, size_t n) {
         __m256i *dVec = (__m256i *)(d);
         const __m256i *sVec = (const __m256i *)(s);
         size_t nVec = n / sizeof(__m256i);
         for (; nVec > 0; nVec--, sVec++, dVec++) {
               const __m256i temp = _mm256_load_si256(sVec);
               _mm256_stream_si256(dVec, temp);
         }
         _mm_sfence();
}


