#include "nnue.h"
#include "search.h"

#include <algorithm>
#include <fstream>
#include <immintrin.h>
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

int NNUE::feature(Color persp, Color color, PieceType p, Square sq) {
    int ci = persp == color ? 0 : 1;
    int sqi = persp == Color::BLACK ? (sq).flip().index() : (sq).index();
    return ci * 64 * 6 + (int)p * 64 + sqi; // Index of the feature
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

void NNUE::randomize() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(-500, 500);
    for (int i = 0; i < H1.size(); ++i) {
        H1[i] = distrib(gen);
    }
    for (int i = 0; i < H1Bias.size(); ++i) {
        H1Bias[i] = distrib(gen);
    }
    for (int i = 0; i < OW.size(); ++i) {
        for (int j = 0; j < OW[i].size(); j++)
            OW[i][j] = distrib(gen);
    }
    for (int i = 0; i < OUTPUT_BUCKETS; i++)
        outputBias[i] = distrib(gen);
}

int NNUE::getOutputBucket(int material) {
    return (material - 2) / (32 / OUTPUT_BUCKETS);
}

int NNUE::inference(Board& board, Accumulator& accumulator) {

    Color stm = board.sideToMove();

    const std::array<int16_t, HL_N>& accumulatorSTM =
        stm == Color::WHITE ? accumulator.white : accumulator.black;
    const std::array<int16_t, HL_N>& accumulatorOPP =
        stm == Color::BLACK ? accumulator.white : accumulator.black;

    // Output buckets are calculated using piececount. Each bucket corresponds
    // to (cnt-2)/(32/N)
    const size_t outputBucket = getOutputBucket(board.occ().count());

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

void Accumulator::refresh(Board& board) {
    Bitboard whiteBB = board.us(Color::WHITE);
    Bitboard blackBB = board.us(Color::BLACK);

    // Obviously the bias is commutative so just add it first
    white = network.H1Bias;
    black = network.H1Bias;

    while (whiteBB) {
        Square sq = whiteBB.pop();

        // White features for both perspectives
        int wf = NNUE::feature(Color::WHITE, Color::WHITE,
                               board.at<PieceType>(sq), sq);
        int bf = NNUE::feature(Color::BLACK, Color::WHITE,
                               board.at<PieceType>(sq), sq);

        for (int i = 0; i < HL_N; i++) {
            // Do the matrix mutliply for the next layer
            white[i] += network.H1[wf * HL_N + i];
            black[i] += network.H1[bf * HL_N + i];
        }
    }

    while (blackBB) {
        Square sq = blackBB.pop();

        // Black features for both perspectives
        int wf = NNUE::feature(Color::WHITE, Color::BLACK,
                               board.at<PieceType>(sq), sq);
        int bf = NNUE::feature(Color::BLACK, Color::BLACK,
                               board.at<PieceType>(sq), sq);

        for (int i = 0; i < HL_N; i++) {
            // Do the matrix mutliply for the next layer
            white[i] += network.H1[wf * HL_N + i];
            black[i] += network.H1[bf * HL_N + i];
        }
    }
}

void Accumulator::print() {
    for (int i = 0; i < HL_N; i++) {
        std::cout << "White: " << white[i] << " Black: " << black[i]
                  << std::endl;
    }
}

// Quiet Accumulation
void Accumulator::quiet(Color stm, Square add, PieceType addPT, Square sub,
                        PieceType subPT) {

    const int addW = NNUE::feature(Color::WHITE, stm, addPT, add);
    const int addB = NNUE::feature(Color::BLACK, stm, addPT, add);

    const int subW = NNUE::feature(Color::WHITE, stm, subPT, sub);
    const int subB = NNUE::feature(Color::BLACK, stm, subPT, sub);

    for (int i = 0; i < HL_N; i++) {
        white[i] += network.H1[addW * HL_N + i] - network.H1[subW * HL_N + i];
        black[i] += network.H1[addB * HL_N + i] - network.H1[subB * HL_N + i];
    }
}
// Capture Accumulation
void Accumulator::capture(Color stm, Square add, PieceType addPT, Square sub1,
                          PieceType subPT1, Square sub2, PieceType subPT2) {
    const int addW = NNUE::feature(Color::WHITE, stm, addPT, add);
    const int addB = NNUE::feature(Color::BLACK, stm, addPT, add);

    const int subW1 = NNUE::feature(Color::WHITE, stm, subPT1, sub1);
    const int subB1 = NNUE::feature(Color::BLACK, stm, subPT1, sub1);

    const int subW2 = NNUE::feature(Color::WHITE, ~stm, subPT2, sub2);
    const int subB2 = NNUE::feature(Color::BLACK, ~stm, subPT2, sub2);

    for (int i = 0; i < HL_N; i++) {
        white[i] += network.H1[addW * HL_N + i] - network.H1[subW1 * HL_N + i] -
                    network.H1[subW2 * HL_N + i];
        black[i] += network.H1[addB * HL_N + i] - network.H1[subB1 * HL_N + i] -
                    network.H1[subB2 * HL_N + i];
    }
}
