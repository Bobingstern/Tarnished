#pragma once
#include <cstdint>

#if defined(__x86_64__) || defined(__amd64__) ||                               \
    (defined(_WIN64) && (defined(_M_X64) || defined(_M_AMD64)))
    #include <immintrin.h>
    #if defined(__AVX512F__)
        #define USE_AVX512
        #pragma message("Using AVX512 NNUE inference")
using nativeVector = __m512i;
using vepi8  = __m512i;
using vepi16 = __m512i;
using vepi32 = __m512i;
using vps32  = __m512;
using v128i  = __m128i;
        #define set1_epi16 _mm512_set1_epi16
        #define set1_epi32 _mm512_set1_epi32
        #define load_epi16 _mm512_load_si512
        #define store_epi16 _mm512_store_si512
        #define min_epi16 _mm512_min_epi16
        #define max_epi16 _mm512_max_epi16
        #define madd_epi16 _mm512_madd_epi16
        #define mullo_epi16 _mm512_mullo_epi16
        #define mulhi_epi16 _mm512_mulhi_epi16  
        #define slli_epi16 _mm512_slli_epi16
        #define add_epi16 _mm512_add_epi16
        #define add_epi32 _mm512_add_epi32
        #define sub_epi16 _mm512_sub_epi16
        #define packus_epi16 _mm512_packus_epi16
        #define reduce_epi32 _mm512_reduce_add_epi32
        #define cvtepi32_ps _mm512_cvtepi32_ps
        #define load_ps _mm512_load_ps
        #define store_ps _mm512_store_ps
        #define set1_ps _mm512_set1_ps
        #define mul_add_ps _mm512_fmadd_ps
        #define zero_ps _mm512_setzero_ps
        #define mul_ps _mm512_mul_ps
        #define min_ps _mm512_min_ps
        #define max_ps _mm512_max_ps
        #define reduce_add_ps _mm512_reduce_add_ps

        inline vepi32 dpbusdx2_epi32(const vepi32 sum, const vepi8 vec0, const vepi8 vec1, const vepi8 vec2, const vepi8 vec3) {
            const vepi16 product16a = _mm512_maddubs_epi16(vec0, vec1);
            const vepi16 product16b = _mm512_maddubs_epi16(vec2, vec3);
            const vepi32 product32  = _mm512_madd_epi16(_mm512_add_epi16(product16a, product16b), _mm512_set1_epi16(1));
            return _mm512_add_epi32(sum, product32);
        }

        inline vepi32 dpbusd_epi32(const vepi32 sum, const vepi8 vec0, const vepi8 vec1) {

            const vepi16 product16 = _mm512_maddubs_epi16(vec0, vec1);
            const vepi32 product32 = _mm512_madd_epi16(product16, _mm512_set1_epi16(1));
            return _mm512_add_epi32(sum, product32);
        }

    #elif defined(__AVX2__)
        #define USE_AVX2
        #pragma message("Using AVX2 NNUE inference")
using nativeVector = __m256i;
using vepi8  = __m256i;
using vepi16 = __m256i;
using vepi32 = __m256i;
using vps32  = __m256;
using v128i  = __m128i;
        #define set1_epi16 _mm256_set1_epi16
        #define set1_epi32 _mm256_set1_epi32
        #define load_epi16 _mm256_load_si256
        #define store_epi16 _mm256_store_si256
        #define min_epi16 _mm256_min_epi16
        #define max_epi16 _mm256_max_epi16
        #define madd_epi16 _mm256_madd_epi16
        #define mullo_epi16 _mm256_mullo_epi16
        #define mulhi_epi16 _mm256_mulhi_epi16
        #define slli_epi16 _mm256_slli_epi16
        #define add_epi16 _mm256_add_epi16
        #define add_epi32 _mm256_add_epi32
        #define sub_epi16 _mm256_sub_epi16
        #define packus_epi16 _mm256_packus_epi16
        #define reduce_epi32                                                   \
            [](nativeVector vec) {                                             \
                __m128i xmm1 = _mm256_extracti128_si256(vec, 1);               \
                __m128i xmm0 = _mm256_castsi256_si128(vec);                    \
                xmm0 = _mm_add_epi32(xmm0, xmm1);                              \
                xmm1 = _mm_shuffle_epi32(xmm0, 238);                           \
                xmm0 = _mm_add_epi32(xmm0, xmm1);                              \
                xmm1 = _mm_shuffle_epi32(xmm0, 85);                            \
                xmm0 = _mm_add_epi32(xmm0, xmm1);                              \
                return _mm_cvtsi128_si32(xmm0);                                \
            }
        #define cvtepi32_ps _mm256_cvtepi32_ps
        #define load_ps _mm256_load_ps
        #define store_ps _mm256_store_ps
        #define set1_ps _mm256_set1_ps
        #define mul_add_ps _mm256_fmadd_ps
        #define zero_ps _mm256_setzero_ps
        #define mul_ps _mm256_mul_ps
        #define min_ps _mm256_min_ps
        #define max_ps _mm256_max_ps

        inline vepi32 dpbusdx2_epi32(const vepi32 sum, const vepi8 vec0, const vepi8 vec1, const vepi8 vec2, const vepi8 vec3) {
            const vepi16 product16a = _mm256_maddubs_epi16(vec0, vec1);
            const vepi16 product16b = _mm256_maddubs_epi16(vec2, vec3);
            const vepi32 product32  = _mm256_madd_epi16(_mm256_add_epi16(product16a, product16b), _mm256_set1_epi16(1));
            return _mm256_add_epi32(sum, product32);
        }

        inline vepi32 dpbusd_epi32(const vepi32 sum, const vepi8 vec0, const vepi8 vec1) {
            const vepi16 product16 = _mm256_maddubs_epi16(vec0, vec1);
            const vepi32 product32 = _mm256_madd_epi16(product16, _mm256_set1_epi16(1));
            return _mm256_add_epi32(sum, product32);
        }
        inline float reduce_add_ps(const vps32 *vecs) {
            const __m256 vec       = _mm256_add_ps(vecs[0], vecs[1]);

            const __m128 upper_128 = _mm256_extractf128_ps(vec, 1);
            const __m128 lower_128 = _mm256_castps256_ps128(vec);
            const __m128 sum_128 = _mm_add_ps(lower_128, upper_128);

            const __m128 upper_64 = _mm_movehl_ps(sum_128, sum_128);
            const __m128 sum_64 = _mm_add_ps(sum_128, upper_64);

            const __m128 upper_32 = _mm_shuffle_ps(sum_64, sum_64, 1);
            const __m128 sum_32 = _mm_add_ss(sum_64, upper_32);

            return _mm_cvtss_f32(sum_32);
        }
    #else
        #define AUTOVEC
    #endif
#else
    #define AUTOVEC

#endif

inline float reduce_add(float *sums, const int length) {
    if (length == 2) return sums[0] + sums[1];
    for (int i = 0; i < length / 2; ++i)
        sums[i] += sums[i + length / 2];

    return reduce_add(sums, length / 2);
}