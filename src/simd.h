#include <cstdint>


#if defined(__x86_64__) || defined(__amd64__) ||                               \
    (defined(_WIN64) && (defined(_M_X64) || defined(_M_AMD64)))
    #include <immintrin.h>
    #if defined(__AVX512F__)
        #define USE_AVX512
        #pragma message("Using AVX512 NNUE inference")
using nativeVector = __m512i;
        #define set1_epi16 _mm512_set1_epi16
        #define load_epi16 _mm512_load_si512
        #define min_epi16 _mm512_min_epi16
        #define max_epi16 _mm512_max_epi16
        #define madd_epi16 _mm512_madd_epi16
        #define mullo_epi16 _mm512_mullo_epi16
        #define add_epi32 _mm512_add_epi32
        #define reduce_epi32 _mm512_reduce_add_epi32
    #elif defined(__AVX2__)
        #pragma message("Using AVX2 NNUE inference")
using nativeVector = __m256i;
        #define set1_epi16 _mm256_set1_epi16
        #define load_epi16 _mm256_load_si256
        #define min_epi16 _mm256_min_epi16
        #define max_epi16 _mm256_max_epi16
        #define madd_epi16 _mm256_madd_epi16
        #define mullo_epi16 _mm256_mullo_epi16
        #define add_epi32 _mm256_add_epi32
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
    #else
        #pragma message("Using SSE NNUE inference")
// Assumes SSE support here
using nativeVector = __m128i;
        #define set1_epi16 _mm_set1_epi16
        #define load_epi16 _mm_load_si128
        #define min_epi16 _mm_min_epi16
        #define max_epi16 _mm_max_epi16
        #define madd_epi16 _mm_madd_epi16
        #define mullo_epi16 _mm_mullo_epi16
        #define add_epi32 _mm_add_epi32
        #define reduce_epi32                                                   \
            [](nativeVector vec) {                                             \
                __m128i xmm1 = _mm_shuffle_epi32(vec, 238);                    \
                vec = _mm_add_epi32(vec, xmm1);                                \
                xmm1 = _mm_shuffle_epi32(vec, 85);                             \
                vec = _mm_add_epi32(vec, xmm1);                                \
                return _mm_cvtsi128_si32(vec);                                 \
            }
    #endif
// POWER / VSX implementation (Power9)
#elif defined(__powerpc64__) || defined(__PPC64__) || defined(__powerpc__)
    #pragma message("Using POWER/VSX NNUE inference")
    #include <altivec.h>
    // avoid the altivec 'vector' macro colliding with std::vector in C++
    #ifdef __cplusplus
    #undef vector
    #undef bool
    #undef pixel
    #endif

    using nativeVector = __vector signed short; // 8 x int16_t (128-bit)
    using accVector    = __vector signed int;   // 4 x int32_t (128-bit)

    // --- intrinsics/macros mapping (keeps call-sites identical to AVX code) ---
    #define set1_epi16(x)    vec_splats((short)(x)) /* splat 16-bit value */ 
    #define load_epi16(ptr)  ((nativeVector)vec_vsx_ld(0, (const short*)(ptr)))
    #define min_epi16        vec_min
    #define max_epi16        vec_max
    #define mullo_epi16      vec_mul
    /* madd: pairwise (a0*b0 + a1*b1) into 32-bit lanes, plus c (we pass zero) */
    #define madd_epi16(a,b)  vec_msum((a),(b), (accVector)vec_splats((int)0))
    #define add_epi32        vec_add

    /* deterministic reduction: store lanes, add as unsigned 32-bit to force
       two's-complement (mod 2^32) wrapping identical to SIMD arithmetic */
    #define reduce_epi32                                                   \
        [](accVector vec)->int32_t {                                       \
            int32_t tmp[4] __attribute__((aligned(16)));                   \
            /* store 128-bit vector -> 4x int32 */                         \
            vec_st(vec, 0, tmp);                                           \
            /* sum as uint32_t to mirror SIMD modular wrap */              \
            uint32_t s = (uint32_t)tmp[0] + (uint32_t)tmp[1] +             \
                         (uint32_t)tmp[2] + (uint32_t)tmp[3];               \
            return (int32_t)s;                                             \
        }
#else
    #define AUTOVEC

#endif