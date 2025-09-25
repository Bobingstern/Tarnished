#include "nnue.h"
#include "search.h"

#include <algorithm>
#include <fstream>
#include <random>

int16_t NNUE::ReLU_(int16_t x) {
    return x < 0 ? 0 : x;
}

int16_t NNUE::CReLU_(int16_t x) {
    if (x < 0)
        return 0;
    return x > QA ? QA : x;
}

int32_t NNUE::SCReLU_(int16_t x) {
    if (x < 0)
        return 0;
    else if (x > QA)
        return QA * QA;
    return x * x;
}
// Thanks Turbulence and Chef
#if defined(__x86_64__) || defined(__amd64__) ||                               \
    (defined(_WIN64) && (defined(_M_X64) || defined(_M_AMD64)))
    #include <immintrin.h>
    #if defined(__AVX512F__)
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
// https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md
// https://cosmo.tardis.ac/files/2024-06-01-nnue.html
// https://github.com/ksw0518/Turbulence_v4/blob/96c9eaaa96afa0e16f16daec9f99cf6018f1e119/Turbulence_v4/Evaluation.cpp#L498
int32_t NNUE::optimizedSCReLU(const std::array<int16_t, HL_N>& STM,
                              const std::array<int16_t, HL_N>& OPP, Color col,
                              size_t bucket) {
    const size_t VECTOR_SIZE = sizeof(nativeVector) / sizeof(int16_t);
    static_assert(HL_N % VECTOR_SIZE == 0,
                  "HL size must be divisible by the native register size of "
                  "your CPU for vectorization to work");
    const nativeVector VEC_QA = set1_epi16(QA);
    const nativeVector VEC_ZERO = set1_epi16(0);

    nativeVector accumulator{};
    for (size_t i = 0; i < HL_N; i += VECTOR_SIZE) {
        // load a SIMD vector of inputs, x
        const nativeVector stmAccumValues =
            load_epi16(reinterpret_cast<const nativeVector*>(&STM[i]));
        const nativeVector nstmAccumValues =
            load_epi16(reinterpret_cast<const nativeVector*>(&OPP[i]));

        // compute the clipped ReLU of the inputs, v
        const nativeVector stmClamped =
            min_epi16(VEC_QA, max_epi16(stmAccumValues, VEC_ZERO));
        const nativeVector nstmClamped =
            min_epi16(VEC_QA, max_epi16(nstmAccumValues, VEC_ZERO));

        // load the weights, w
        const nativeVector stmWeights =
            load_epi16(reinterpret_cast<const nativeVector*>(&OW[bucket][i]));
        const nativeVector nstmWeights = load_epi16(
            reinterpret_cast<const nativeVector*>(&OW[bucket][i + HL_N]));

        // SCReLU it
        const nativeVector stmActivated =
            madd_epi16(stmClamped, mullo_epi16(stmClamped, stmWeights));
        const nativeVector nstmActivated =
            madd_epi16(nstmClamped, mullo_epi16(nstmClamped, nstmWeights));

        accumulator = add_epi32(accumulator, stmActivated);
        accumulator = add_epi32(accumulator, nstmActivated);
    }
    return reduce_epi32(accumulator);
}

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

int32_t NNUE::optimizedSCReLU(const std::array<int16_t, HL_N>& STM,
                              const std::array<int16_t, HL_N>& OPP, Color col,
                              size_t bucket) {
    const size_t VECTOR_SIZE = sizeof(nativeVector) / sizeof(int16_t);
    static_assert(HL_N % VECTOR_SIZE == 0,
                  "HL size must be divisible by the native register size of "
                  "your CPU for vectorization to work");

    const nativeVector VEC_QA   = set1_epi16(QA);
    const nativeVector VEC_ZERO = set1_epi16(0);

    accVector accumulator = (accVector)vec_splats((int)0); // zero accumulator

    for (size_t i = 0; i < HL_N; i += VECTOR_SIZE) {
        // load a SIMD vector of inputs, x (8 x int16)
        const nativeVector stmAccumValues =
            load_epi16(&STM[i]);
        const nativeVector nstmAccumValues =
            load_epi16(&OPP[i]);

        // clamp to [0, QA]
        const nativeVector stmClamped =
            min_epi16(VEC_QA, max_epi16(stmAccumValues, VEC_ZERO));
        const nativeVector nstmClamped =
            min_epi16(VEC_QA, max_epi16(nstmAccumValues, VEC_ZERO));

        // load weights (assumes OW[bucket] is int16_t[] with 2*HL_N entries)
        const nativeVector stmWeights =
            load_epi16(&OW[bucket][i]);
        const nativeVector nstmWeights =
            load_epi16(&OW[bucket][i + HL_N]);

        // perform SCReLU: v * (v * w) pairwise-accumulated to 32-bit lanes
        const accVector stmActivated =
            madd_epi16(stmClamped, mullo_epi16(stmClamped, stmWeights));
        const accVector nstmActivated =
            madd_epi16(nstmClamped, mullo_epi16(nstmClamped, nstmWeights));

        accumulator = add_epi32(accumulator, stmActivated);
        accumulator = add_epi32(accumulator, nstmActivated);
    }

    return reduce_epi32(accumulator);
}

#else

int32_t NNUE::optimizedSCReLU(const std::array<int16_t, HL_N>& STM,
                              const std::array<int16_t, HL_N>& OPP, Color col,
                              size_t bucket) {
    int32_t eval = 0;
    for (int i = 0; i < HL_N; i++) {
        eval += SCReLU_(STM[i]) * OW[bucket][i];
        eval += SCReLU_(OPP[i]) * OW[bucket][HL_N + i];
    }
    return eval;
}

#endif

int NNUE::feature(Color persp, Color color, PieceType p, Square sq, Square king) {
    int ci = persp == color ? 0 : 1;
    int sqi = persp == Color::BLACK ? sq.flip().index() : sq.index();
    if (king.file() >= File::FILE_E && HORIZONTAL_MIRROR)
        sqi ^= 7;
    return Accumulator::kingBucket(king, persp) * 768 + ci * 64 * 6 + int(p) * 64 + sqi; // Index of the feature
}

void NNUE::load(const std::string& file) {
    std::ifstream stream(file, std::ios::binary);

    for (int i = 0; i < H1.size(); ++i) {
        H1[i] = readLittleEndian<int16_t>(stream);
    }
    for (int i = 0; i < H1Bias.size(); ++i) {
        H1Bias[i] = readLittleEndian<int16_t>(stream);
    }
    for (int i = 0; i < OW.size(); ++i) {
        for (int j = 0; j < OW[i].size(); j++)
            OW[i][j] = readLittleEndian<int16_t>(stream);
    }
    for (int i = 0; i < OUTPUT_BUCKETS; i++)
        outputBias[i] = readLittleEndian<int16_t>(stream);
}


int NNUE::inference(Board& board, Accumulator& accumulator) {

    Color stm = board.sideToMove();

    const std::array<int16_t, HL_N>& accumulatorSTM =
        stm == Color::WHITE ? accumulator.white : accumulator.black;
    const std::array<int16_t, HL_N>& accumulatorOPP =
        stm == Color::BLACK ? accumulator.white : accumulator.black;

    // Output buckets are calculated using piececount. Each bucket corresponds
    // to (cnt-2)/(32/N)
    const size_t outputBucket =
        (board.occ().count() - 2) / (32 / OUTPUT_BUCKETS);

    int64_t eval = 0;

    if (ACTIVATION != SCReLU) {

        for (int i = 0; i < HL_N; i++) {
            if (ACTIVATION == CReLU) {
                eval += CReLU_(accumulatorSTM[i]) * OW[outputBucket][i];
                eval += CReLU_(accumulatorOPP[i]) * OW[outputBucket][HL_N + i];
            }
        }
    } else {
        eval =
            optimizedSCReLU(accumulatorSTM, accumulatorOPP, stm, outputBucket);
        eval /= QA;
    }

    eval += outputBias[outputBucket];
    return (eval * NNUE_SCALE) / (QA * QB);
}

// ------ Accumulator -------

bool Accumulator::needRefresh(Move kingMove, Color stm) {
    if (!HORIZONTAL_MIRROR)
        return false;

    Square from = kingMove.from();
    Square to = kingMove.to();

    if (kingMove.typeOf() == Move::CASTLING) {
        Square king = kingMove.from();
        Square standardKing = stm == Color::WHITE ? Square::SQ_E1 : Square::SQ_E8; // For chess960
        to = (king > kingMove.to()) ? standardKing - 2 : standardKing + 2;
    }

    if ((from.file() >= File::FILE_E) != (to.file() >= File::FILE_E))
        return true;
    if (kingBucket(from, stm) != kingBucket(to, stm))
        return true;

    return false;
}

int Accumulator::kingBucket(Square kingSq, Color color) {
    return BUCKET_LAYOUT[(kingSq ^ (int(color) * 56)).index()];
}

void Accumulator::refresh(Board& board, Color persp) {
    Bitboard whiteBB = board.us(Color::WHITE);
    Bitboard blackBB = board.us(Color::BLACK);

    auto& accPerspective = persp == Color::WHITE ? white : black;

    // Obviously the bias is commutative so just add it first
    accPerspective = network.H1Bias;

    Square kingSq = board.kingSq(persp);

    while (whiteBB) {
        Square sq = whiteBB.pop();

        // White features for both perspectives
        int feature = NNUE::feature(persp, Color::WHITE,
                               board.at<PieceType>(sq), sq, kingSq);

        for (int i = 0; i < HL_N; i++) {
            // Do the matrix mutliply for the next layer
            accPerspective[i] += network.H1[feature * HL_N + i];
        }
    }

    while (blackBB) {
        Square sq = blackBB.pop();

        // Black features for both perspectives
        int feature = NNUE::feature(persp, Color::BLACK,
                               board.at<PieceType>(sq), sq, kingSq);
        
        for (int i = 0; i < HL_N; i++) {
            // Do the matrix mutliply for the next layer
            accPerspective[i] += network.H1[feature * HL_N + i];
        }
    }
}

// Refresh with cache
void Accumulator::refresh(Board& board, Color persp, InputBucketCache& bucketCache) {
    Square kingSq = board.kingSq(persp);

    auto& accPerspective = persp == Color::WHITE ? white : black;

    BucketCacheEntry& cache = bucketCache.cache[int(persp)][kingSq.file() >= File::FILE_E][kingBucket(kingSq, persp)];

    if (!cache.isInit) {
        cache.features = network.H1Bias;
        cache.isInit = true;
    }

    accPerspective = cache.features;

    for (Color c : {Color::WHITE, Color::BLACK}) {
        for (PieceType pt : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, 
                        PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            Bitboard cachedPieces = cache.cachedPieces[int(pt)] & cache.cachedPieces[int(c) + 6];
            Bitboard added = board.pieces(pt, c) & ~cachedPieces;
            Bitboard subbed = ~board.pieces(pt, c) & cachedPieces;

            while (added) {
                Square sq = added.pop();
                int feature = NNUE::feature(persp, c, pt, sq, kingSq);

                for (int i = 0; i < HL_N; i++) {
                    // Do the matrix mutliply for the next layer
                    accPerspective[i] += network.H1[feature * HL_N + i];
                }
            }

            while (subbed) {
                Square sq = subbed.pop();
                int feature = NNUE::feature(persp, c, pt, sq, kingSq);

                for (int i = 0; i < HL_N; i++) {
                    // Do the matrix mutliply for the next layer
                    accPerspective[i] -= network.H1[feature * HL_N + i];
                }
            }
        }
    }

    cache.set(board, accPerspective);
    
}

void Accumulator::refresh(Board& board) {
    refresh(board, Color::WHITE);
    refresh(board, Color::BLACK);
}

void Accumulator::print() {
    for (int i = 0; i < HL_N; i++) {
        std::cout << "White: " << white[i] << " Black: " << black[i]
                  << std::endl;
    }
}


void Accumulator::addPiece(Board& board, Color stm, Color persp, Square add, PieceType addPT) {
    Square kingSq = board.kingSq(persp);
    auto& accPerspective = persp == Color::WHITE ? white : black;

    const int feature = NNUE::feature(persp, stm, addPT, add, kingSq);

    for (int i = 0; i < HL_N; i++) {
        accPerspective[i] += network.H1[feature * HL_N + i];
    }
}

void Accumulator::subPiece(Board& board, Color stm, Color persp, Square sub, PieceType subPT) {
    Square kingSq = board.kingSq(persp);
    auto& accPerspective = persp == Color::WHITE ? white : black;

    const int feature = NNUE::feature(persp, stm, subPT, sub, kingSq);

    for (int i = 0; i < HL_N; i++) {
        accPerspective[i] -= network.H1[feature * HL_N + i];
    }
}

void Accumulator::addPiece(Board& board, Color stm, Square add, PieceType addPT) {
    addPiece(board, stm, Color::WHITE, add, addPT);
    addPiece(board, stm, Color::BLACK, add, addPT);
}

void Accumulator::subPiece(Board& board, Color stm, Square sub, PieceType subPT) {
    subPiece(board, stm, Color::WHITE, sub, subPT);
    subPiece(board, stm, Color::BLACK, sub, subPT);
}

// Quiet Accumulation
void Accumulator::quiet(Board& board, Color stm, Square add, PieceType addPT, Square sub,
                        PieceType subPT) {

    Square whiteKing = board.kingSq(Color::WHITE);
    Square blackKing = board.kingSq(Color::BLACK);

    const int addW = NNUE::feature(Color::WHITE, stm, addPT, add, whiteKing);
    const int addB = NNUE::feature(Color::BLACK, stm, addPT, add, blackKing);

    const int subW = NNUE::feature(Color::WHITE, stm, subPT, sub, whiteKing);
    const int subB = NNUE::feature(Color::BLACK, stm, subPT, sub, blackKing);

    for (int i = 0; i < HL_N; i++) {
        white[i] += network.H1[addW * HL_N + i] - network.H1[subW * HL_N + i];
        black[i] += network.H1[addB * HL_N + i] - network.H1[subB * HL_N + i];
    }
}
// Capture Accumulation
void Accumulator::capture(Board& board, Color stm, Square add, PieceType addPT, Square sub1,
                          PieceType subPT1, Square sub2, PieceType subPT2) {

    Square whiteKing = board.kingSq(Color::WHITE);
    Square blackKing = board.kingSq(Color::BLACK);

    const int addW = NNUE::feature(Color::WHITE, stm, addPT, add, whiteKing);
    const int addB = NNUE::feature(Color::BLACK, stm, addPT, add, blackKing);

    const int subW1 = NNUE::feature(Color::WHITE, stm, subPT1, sub1, whiteKing);
    const int subB1 = NNUE::feature(Color::BLACK, stm, subPT1, sub1, blackKing);

    const int subW2 = NNUE::feature(Color::WHITE, ~stm, subPT2, sub2, whiteKing);
    const int subB2 = NNUE::feature(Color::BLACK, ~stm, subPT2, sub2, blackKing);

    for (int i = 0; i < HL_N; i++) {
        white[i] += network.H1[addW * HL_N + i] - network.H1[subW1 * HL_N + i] -
                    network.H1[subW2 * HL_N + i];
        black[i] += network.H1[addB * HL_N + i] - network.H1[subB1 * HL_N + i] -
                    network.H1[subB2 * HL_N + i];
    }
}
