#pragma once
#include <cstdint>

#if defined(__x86_64__) || defined(__amd64__) ||                               \
    (defined(_WIN64) && (defined(_M_X64) || defined(_M_AMD64)))
    #include <immintrin.h>
    #if defined(__AVX512F__)
        #define USE_AVX512
using nativeVector = __m512i;
using vepi8  = __m512i;
using vepi16 = __m512i;
using vepi32 = __m512i;
using vps32  = __m512;
using v128i  = __m128i;
        inline vepi16 set1_epi16(int16_t v){ return _mm512_set1_epi16(v); }
        inline vepi16 load_epi16(const vepi16* p){ return _mm512_load_si512(p); }
        inline void   store_epi16(vepi16* p, vepi16 v){ _mm512_store_si512(p,v); }

        inline vepi16 min_epi16(vepi16 a, vepi16 b){ return _mm512_min_epi16(a,b); }
        inline vepi16 max_epi16(vepi16 a, vepi16 b){ return _mm512_max_epi16(a,b); }

        inline vepi16 mullo_epi16(vepi16 a, vepi16 b){ return _mm512_mullo_epi16(a,b); }
        inline vepi16 mulhi_epi16(vepi16 a, vepi16 b){ return _mm512_mulhi_epi16(a,b); }

        inline vepi16 slli_epi16(vepi16 v,int imm){ return _mm512_slli_epi16(v,imm); }

        inline vepi16 add_epi16(vepi16 a, vepi16 b){ return _mm512_add_epi16(a,b); }
        inline vepi16 sub_epi16(vepi16 a, vepi16 b){ return _mm512_sub_epi16(a,b); }

        inline vepi16 packus_epi16(vepi16 a, vepi16 b){
            return _mm512_packus_epi16(a,b);
        }

        inline vepi32 set1_epi32(int32_t v){ return _mm512_set1_epi32(v); }

        inline vepi32 madd_epi16(vepi16 a, vepi16 b){
            return _mm512_madd_epi16(a,b);   // epi16 → epi32
        }

        inline vepi32 add_epi32(vepi32 a, vepi32 b){
            return _mm512_add_epi32(a,b);
        }

        inline int reduce_epi32(vepi32 v){
            return _mm512_reduce_add_epi32(v);
        }

        inline vps32 cvtepi32_ps(vepi32 v){ return _mm512_cvtepi32_ps(v); }

        inline vps32 load_ps(const float* p){ return _mm512_load_ps(p); }
        inline void  store_ps(float* p,vps32 v){ _mm512_store_ps(p,v); }

        inline vps32 set1_ps(float v){ return _mm512_set1_ps(v); }
        inline vps32 zero_ps(){ return _mm512_setzero_ps(); }

        inline vps32 mul_ps(vps32 a,vps32 b){ return _mm512_mul_ps(a,b); }

        inline vps32 mul_add_ps(vps32 a,vps32 b,vps32 c){
            return _mm512_fmadd_ps(a,b,c);
        }

        inline vps32 min_ps(vps32 a,vps32 b){ return _mm512_min_ps(a,b); }
        inline vps32 max_ps(vps32 a,vps32 b){ return _mm512_max_ps(a,b); }
        inline float reduce_add_ps(const vps32 *vecs) { return _mm512_reduce_add_ps(vecs[0]); }

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

        using nativeVector = __m256i;
        using vepi8  = __m256i;
        using vepi16 = __m256i;
        using vepi32 = __m256i;
        using vps32  = __m256;
        using v128i  = __m128i;
        inline vepi16 set1_epi16(int16_t v){ return _mm256_set1_epi16(v); }
        inline vepi16 load_epi16(const vepi16* p){ return _mm256_load_si256(p); }
        inline void   store_epi16(vepi16* p, vepi16 v){ _mm256_store_si256(p,v); }

        inline vepi16 min_epi16(vepi16 a, vepi16 b){ return _mm256_min_epi16(a,b); }
        inline vepi16 max_epi16(vepi16 a, vepi16 b){ return _mm256_max_epi16(a,b); }

        inline vepi16 mullo_epi16(vepi16 a, vepi16 b){ return _mm256_mullo_epi16(a,b); }
        inline vepi16 mulhi_epi16(vepi16 a, vepi16 b){ return _mm256_mulhi_epi16(a,b); }

        inline vepi16 slli_epi16(vepi16 v,int imm){ return _mm256_slli_epi16(v,imm); }

        inline vepi16 add_epi16(vepi16 a, vepi16 b){ return _mm256_add_epi16(a,b); }
        inline vepi16 sub_epi16(vepi16 a, vepi16 b){ return _mm256_sub_epi16(a,b); }

        inline vepi16 packus_epi16(vepi16 a, vepi16 b){
            return _mm256_packus_epi16(a,b);
        }

        inline vepi32 set1_epi32(int32_t v){ return _mm256_set1_epi32(v); }

        inline vepi32 madd_epi16(vepi16 a, vepi16 b){
            return _mm256_madd_epi16(a,b);   // produces epi32 lanes
        }

        inline vepi32 add_epi32(vepi32 a, vepi32 b){
            return _mm256_add_epi32(a,b);
        }

        inline int reduce_epi32(vepi32 v){
            v128i hi=_mm256_extracti128_si256(v,1);
            v128i lo=_mm256_castsi256_si128(v);
            lo=_mm_add_epi32(lo,hi);
            hi=_mm_shuffle_epi32(lo,238);
            lo=_mm_add_epi32(lo,hi);
            hi=_mm_shuffle_epi32(lo,85);
            lo=_mm_add_epi32(lo,hi);
            return _mm_cvtsi128_si32(lo);
        }

        inline vps32 cvtepi32_ps(vepi32 v){ return _mm256_cvtepi32_ps(v); }

        inline vps32 load_ps(const float* p){ return _mm256_load_ps(p); }
        inline void  store_ps(float* p,vps32 v){ _mm256_store_ps(p,v); }

        inline vps32 set1_ps(float v){ return _mm256_set1_ps(v); }
        inline vps32 zero_ps(){ return _mm256_setzero_ps(); }

        inline vps32 mul_ps(vps32 a,vps32 b){ return _mm256_mul_ps(a,b); }
        inline vps32 mul_add_ps(vps32 a,vps32 b,vps32 c){ return _mm256_fmadd_ps(a,b,c); }

        inline vps32 min_ps(vps32 a,vps32 b){ return _mm256_min_ps(a,b); }
        inline vps32 max_ps(vps32 a,vps32 b){ return _mm256_max_ps(a,b); }


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