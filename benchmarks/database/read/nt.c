#include <immintrin.h>

void _avx_async_cpy(void *d, const void *s, size_t n) {
         __m512i *dVec = (__m512i *)(d);
         const __m512i *sVec = (const __m512i *)(s);
         size_t nVec = n / sizeof(__m512i);
         for (; nVec > 0; nVec--, sVec++, dVec++) {
               const __m512i temp = _mm512_load_si512(sVec);
               _mm512_store_si512(dVec, temp);
         }
         _mm_sfence();
}


