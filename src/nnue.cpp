#include "nnue.h"
#include "search.h"
#include "parameters.h"

#include <algorithm>
#include <fstream>
#include <random>

QuantisedNetwork quantisedNet;
UnquantisedNetwork unquantisedNet;
const Network* permutedNet;

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


void quantise_raw() {
    // https://github.com/PGG106/Alexandria/blob/fdf5dbd744ea601f9d7dbedcc9dfe1c391aba37c/src/nnue.cpp#L32
    std::ifstream stream{"raw.bin", std::ios::binary};

    stream.read(reinterpret_cast<char *>(&unquantisedNet), sizeof(UnquantisedNetwork));

    // // Merge factoriser  + quantise FT weights
    for (int bucket = 0; bucket < INPUT_BUCKETS; ++bucket) {
        int bucket_offset = bucket * (768 * L1_SIZE);

        for (int i = 0; i < 768 * L1_SIZE; ++i) {
            float w = unquantisedNet.FTWeights[bucket_offset + i] + unquantisedNet.Factoriser[i];

            quantisedNet.FTWeights[bucket_offset + i] = static_cast<int16_t>(std::round(w * QA));
        }
    }

    // // Quantise FT Biases
    for (int i = 0; i < L1_SIZE; ++i)
        quantisedNet.FTBiases[i] = static_cast<int16_t>(std::round(unquantisedNet.FTBiases[i] * QA));

    // // Quantise L1, L2 and L3 weights and biases
    for (int bucket = 0; bucket < OUTPUT_BUCKETS; ++bucket) {
        // Quantise L1 Weights
        for (int i = 0; i < L1_SIZE; ++i)
            for (int j = 0; j < L2_SIZE; ++j)
                quantisedNet.L1Weights[i][bucket][j] = static_cast<int8_t>(std::round(
                    unquantisedNet.L1Weights[i][bucket][j] * 64));

        // Quantise L1 Biases
        for (int i = 0; i < L2_SIZE; ++i) {
            quantisedNet.L1Biases[bucket][i] = unquantisedNet.L1Biases[bucket][i];
        }

        // Quantise L2 Weights
        for (int i = 0; i < L2_SIZE; ++i)
            for (int j = 0; j < L3_SIZE; ++j)
                quantisedNet.L2Weights[i][bucket][j] = unquantisedNet.L2Weights[i][bucket][j];

        // Quantise L2 Biases
        for (int i = 0; i < L3_SIZE; ++i)
            quantisedNet.L2Biases[bucket][i] = unquantisedNet.L2Biases[bucket][i];

        // Quantise L3 Weights
        for (int i = 0; i < L3_SIZE; ++i)
            quantisedNet.L3Weights[i][bucket] = unquantisedNet.L3Weights[i][bucket];

        // Quantise L3 Biases
        quantisedNet.L3Biases[bucket] = unquantisedNet.L3Biases[bucket];
    }

    std::ofstream out{"quantised.bin", std::ios::binary};
    out.write(reinterpret_cast<const char *>(&quantisedNet), sizeof(QuantisedNetwork));
    std::cout << "Successfully Quantised" << std::endl;
}

// https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md
// https://cosmo.tardis.ac/files/2024-06-01-nnue.html
// https://github.com/ksw0518/Turbulence_v4/blob/96c9eaaa96afa0e16f16daec9f99cf6018f1e119/Turbulence_v4/Evaluation.cpp#L498
#ifndef AUTOVEC
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

int NNUE::feature(Color persp, Color color, PieceType p, Square sq, Square king) {
    int ci = persp == color ? 0 : 1;
    int sqi = persp == Color::BLACK ? sq.flip().index() : sq.index();
    if (king.file() >= File::FILE_E && HORIZONTAL_MIRROR)
        sqi ^= 7;
    return Accumulator::kingBucket(king, persp) * 768 + ci * 64 * 6 + int(p) * 64 + sqi; // Index of the feature
}


void NNUE::activateL1(Accumulator& acc, Color col, uint8_t* output) {
    const std::array<int16_t, HL_N>& stm =
        col == Color::WHITE ? acc.white : acc.black;
    const std::array<int16_t, HL_N>& opp =
        col == Color::BLACK ? acc.white : acc.black;

    for (int i = 0; i < L1_SIZE / 2; i++) {
        int16_t c0 = std::clamp<int16_t>(stm[i], 0, QA);
        int16_t c1 = std::clamp<int16_t>(stm[i + L1_SIZE / 2], 0, QA);
        output[i] = static_cast<uint8_t>(c0 * c1 >> FT_SHIFT);

        c0 = std::clamp<int16_t>(opp[i], 0, QA);
        c1 = std::clamp<int16_t>(opp[i + L1_SIZE / 2], 0, QA);
        output[i + L1_SIZE / 2] = static_cast<uint8_t>(c0 * c1 >> FT_SHIFT);
    }
}

inline float reduce_add(float *sums, const int length) {
    if (length == 2) return sums[0] + sums[1];
    for (int i = 0; i < length / 2; ++i)
        sums[i] += sums[i + length / 2];

    return reduce_add(sums, length / 2);
}

void NNUE::forwardL1(const uint8_t* inputs, const int8_t* weights, const float* biases, float* output) {
    int sums[L2_SIZE] = {0};
    for (int i = 0; i < L1_SIZE; ++i) {
        for (int j = 0; j < L2_SIZE; ++j) {
            sums[j] += static_cast<int32_t>(inputs[i] * weights[j * L1_SIZE + i]);
        }
    }

    for (int i = 0; i < L2_SIZE; ++i) {
        float c = std::clamp(float(sums[i]) * L1_MUL + biases[i], 0.0f, 1.0f);
        output[i] = c * c;
    }
}

void NNUE::forwardL2(const float* inputs, const float* weights, const float* biases, float* output) {
    float sums[L3_SIZE];

    for (int i = 0; i < L3_SIZE; ++i)
        sums[i] = biases[i];

    for (int i = 0; i < L2_SIZE; ++i) {
        const float *weight = &weights[i * L3_SIZE];
        for (int out = 0; out < L3_SIZE; ++out) {
            sums[out] += inputs[i] * weight[out];
        }
    }

    for (int i = 0; i < L3_SIZE; ++i) {
        float c = std::clamp(sums[i], 0.0f, 1.0f);
        output[i] = c * c;
    }
}

void NNUE::forwardL3(const float* inputs, const float* weights, const float bias, float& output) {
    constexpr int avx512chunk = 512 / 32;
    constexpr int numSums = avx512chunk;
    float sums[numSums] = {0};

    // Affine transform for L3
    for (int i = 0; i < L3_SIZE; ++i) {
        sums[i % numSums] += inputs[i] * weights[i];
    }
    output = reduce_add(sums, numSums) + bias;
}

int NNUE::inference(Board& board, Accumulator& accumulator) {

    Color stm = board.sideToMove();
    const size_t outputBucket = (board.occ().count() - 2) / (32 / OUTPUT_BUCKETS);
    alignas (64) uint8_t FTOutputs[L1_SIZE];
    alignas (64) float L1Outputs[L2_SIZE];
    alignas (64) float L2Outputs[L3_SIZE];
    float output;

    activateL1(accumulator, stm, FTOutputs);
    forwardL1(FTOutputs, permutedNet->L1Weights[outputBucket], permutedNet->L1Biases[outputBucket], L1Outputs);
    forwardL2(L1Outputs, permutedNet->L2Weights[outputBucket], permutedNet->L2Biases[outputBucket], L2Outputs);
    forwardL3(L2Outputs, permutedNet->L3Weights[outputBucket], permutedNet->L3Biases[outputBucket], output);
    return output * NNUE_SCALE;
    // Color stm = board.sideToMove();

    // const std::array<int16_t, HL_N>& accumulatorSTM =
    //     stm == Color::WHITE ? accumulator.white : accumulator.black;
    // const std::array<int16_t, HL_N>& accumulatorOPP =
    //     stm == Color::BLACK ? accumulator.white : accumulator.black;

    // // Output buckets are calculated using piececount. Each bucket corresponds
    // // to (cnt-2)/(32/N)
    // const size_t outputBucket =
    //     (board.occ().count() - 2) / (32 / OUTPUT_BUCKETS);

    // int64_t eval = 0;

    // if (ACTIVATION != SCReLU) {

    //     for (int i = 0; i < HL_N; i++) {
    //         if (ACTIVATION == CReLU) {
    //             eval += CReLU_(accumulatorSTM[i]) * OW[outputBucket][i];
    //             eval += CReLU_(accumulatorOPP[i]) * OW[outputBucket][HL_N + i];
    //         }
    //     }
    // } else {
    //     eval =
    //         optimizedSCReLU(accumulatorSTM, accumulatorOPP, stm, outputBucket);
    //     eval /= QA;
    // }

    // eval += outputBias[outputBucket];
    // return (eval * NNUE_SCALE) / (QA * QB);
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

    needsRefresh[int(persp)] = false;
    computed[int(persp)] = true;

    // Obviously the bias is commutative so just add it first
    accPerspective = std::to_array(permutedNet->FTBiases);

    Square kingSq = board.kingSq(persp);

    while (whiteBB) {
        Square sq = whiteBB.pop();

        // White features for both perspectives
        int feature = NNUE::feature(persp, Color::WHITE,
                               board.at<PieceType>(sq), sq, kingSq);

        for (int i = 0; i < HL_N; i++) {
            // Do the matrix mutliply for the next layer
            accPerspective[i] += permutedNet->FTWeights[feature * HL_N + i];
        }
    }

    while (blackBB) {
        Square sq = blackBB.pop();

        // Black features for both perspectives
        int feature = NNUE::feature(persp, Color::BLACK,
                               board.at<PieceType>(sq), sq, kingSq);
        
        for (int i = 0; i < HL_N; i++) {
            // Do the matrix mutliply for the next layer
            accPerspective[i] += permutedNet->FTWeights[feature * HL_N + i];
        }
    }
}

// Refresh with cache
void Accumulator::refresh(Board& board, Color persp, InputBucketCache& bucketCache) {
    Square kingSq = board.kingSq(persp);

    auto& accPerspective = persp == Color::WHITE ? white : black;

    needsRefresh[int(persp)] = false;
    computed[int(persp)] = true;

    BucketCacheEntry& cache = bucketCache.cache[int(persp)][kingSq.file() >= File::FILE_E][kingBucket(kingSq, persp)];

    if (!cache.isInit) {
        cache.features = std::to_array(permutedNet->FTBiases);
        cache.isInit = true;
    }

    accPerspective = cache.features;

    int addIndex = 0;
    int subIndex = 0;
    std::array<int, 32> adds = {};
    std::array<int, 32> subs = {};

    for (Color c : {Color::WHITE, Color::BLACK}) {
        for (PieceType pt : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, 
                        PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            Bitboard cachedPieces = cache.cachedPieces[int(pt)] & cache.cachedPieces[int(c) + 6];
            Bitboard added = board.pieces(pt, c) & ~cachedPieces;
            Bitboard subbed = ~board.pieces(pt, c) & cachedPieces;

            while (added) {
                Square sq = added.pop();
                int feature = NNUE::feature(persp, c, pt, sq, kingSq);
                adds[addIndex] = feature;
                addIndex++;
            }

            while (subbed) {
                Square sq = subbed.pop();
                int feature = NNUE::feature(persp, c, pt, sq, kingSq);
                subs[subIndex] = feature;
                subIndex++;
            }
        }
    }
    // add in batches of 4
    while (addIndex >= 4) {
        refreshAdd4(accPerspective, adds[addIndex - 1], adds[addIndex - 2], adds[addIndex - 3], adds[addIndex - 4]);
        addIndex -= 4;
    }
    // add remaining individually
    while (addIndex > 0) {
        for (int i = 0; i < HL_N; i++) {
            accPerspective[i] += permutedNet->FTWeights[adds[addIndex - 1] * HL_N + i];
        }
        addIndex--;
    }
    // sub in batches of 4
    while (subIndex >= 4) {
        refreshSub4(accPerspective, subs[subIndex - 1], subs[subIndex - 2], subs[subIndex - 3], subs[subIndex - 4]);
        subIndex -= 4;
    }
    // sub remaining individually
    while (subIndex > 0) {
        for (int i = 0; i < HL_N; i++) {
            accPerspective[i] -= permutedNet->FTWeights[subs[subIndex - 1] * HL_N + i];
        }
        subIndex--;
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


void Accumulator::refreshAdd4(std::array<int16_t, HL_N>& acc, int add0, int add1, int add2, int add3) {
    for (int i = 0; i < HL_N; i++) {
        acc[i] += permutedNet->FTWeights[add0 * HL_N + i];
        acc[i] += permutedNet->FTWeights[add1 * HL_N + i];
        acc[i] += permutedNet->FTWeights[add2 * HL_N + i];
        acc[i] += permutedNet->FTWeights[add3 * HL_N + i];
    }
}
void Accumulator::refreshSub4(std::array<int16_t, HL_N>& acc, int sub0, int sub1, int sub2, int sub3) {
    for (int i = 0; i < HL_N; i++) {
        acc[i] -= permutedNet->FTWeights[sub0 * HL_N + i];
        acc[i] -= permutedNet->FTWeights[sub1 * HL_N + i];
        acc[i] -= permutedNet->FTWeights[sub2 * HL_N + i];
        acc[i] -= permutedNet->FTWeights[sub3 * HL_N + i];
    }
}

void Accumulator::applyDelta(Color persp, Accumulator& prev) {
    FeatureDelta delta = featureDeltas[int(persp)];
    if (persp == Color::WHITE)
        white = prev.white;
    else
        black = prev.black;
    if (delta.adds == 1 && delta.subs == 1)
        addSubDelta(persp, delta.toAdd[0], delta.toSub[0]);
    else if (delta.adds == 1 && delta.subs == 2)
        addSubSubDelta(persp, delta.toAdd[0], delta.toSub[0], delta.toSub[1]);
    else if (delta.adds == 2 && delta.subs == 2)
        addAddSubSubDelta(persp, delta.toAdd[0], delta.toAdd[1], delta.toSub[0], delta.toSub[1]);

    computed[int(persp)] = true;

}
void Accumulator::addSubDelta(Color persp, int addF, int subF) {
    auto& accPerspective = persp == Color::WHITE ? white : black;
    for (int i = 0; i < HL_N; i++) {
        accPerspective[i] += permutedNet->FTWeights[addF * HL_N + i];
        accPerspective[i] -= permutedNet->FTWeights[subF * HL_N + i];
    }
}
void Accumulator::addSubSubDelta(Color persp, int addF, int subF1, int subF2) {
    auto& accPerspective = persp == Color::WHITE ? white : black;
    for (int i = 0; i < HL_N; i++) {
        accPerspective[i] += permutedNet->FTWeights[addF * HL_N + i];
        accPerspective[i] -= permutedNet->FTWeights[subF1 * HL_N + i];
        accPerspective[i] -= permutedNet->FTWeights[subF2 * HL_N + i];
    }
}
void Accumulator::addAddSubSubDelta(Color persp, int addF1, int addF2, int subF1, int subF2) {
    auto& accPerspective = persp == Color::WHITE ? white : black;
    for (int i = 0; i < HL_N; i++) {
        accPerspective[i] += permutedNet->FTWeights[addF1 * HL_N + i];
        accPerspective[i] += permutedNet->FTWeights[addF2 * HL_N + i];
        accPerspective[i] -= permutedNet->FTWeights[subF1 * HL_N + i];
        accPerspective[i] -= permutedNet->FTWeights[subF2 * HL_N + i];
    }
}

void Accumulator::addPiece(Board& board, Color stm, Color persp, Square add, PieceType addPT) {
    Square kingSq = board.kingSq(persp);
    auto& accPerspective = persp == Color::WHITE ? white : black;
    const int feature = NNUE::feature(persp, stm, addPT, add, kingSq);

    FeatureDelta& delta = featureDeltas[int(persp)];
    delta.toAdd[delta.adds++] = feature;
}

void Accumulator::subPiece(Board& board, Color stm, Color persp, Square sub, PieceType subPT) {
    Square kingSq = board.kingSq(persp);
    auto& accPerspective = persp == Color::WHITE ? white : black;
    const int feature = NNUE::feature(persp, stm, subPT, sub, kingSq);

    FeatureDelta& delta = featureDeltas[int(persp)];
    delta.toSub[delta.subs++] = feature;
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

    FeatureDelta& deltaW = featureDeltas[0];
    FeatureDelta& deltaB = featureDeltas[1];

    deltaW.toAdd[deltaW.adds++] = addW;
    deltaB.toAdd[deltaB.adds++] = addB;

    deltaW.toSub[deltaW.subs++] = subW;
    deltaB.toSub[deltaB.subs++] = subB;
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

    FeatureDelta& deltaW = featureDeltas[0];
    FeatureDelta& deltaB = featureDeltas[1];

    deltaW.toAdd[deltaW.adds++] = addW;
    deltaB.toAdd[deltaB.adds++] = addB;

    deltaW.toSub[deltaW.subs++] = subW1;
    deltaB.toSub[deltaB.subs++] = subB1;

    deltaW.toSub[deltaW.subs++] = subW2;
    deltaB.toSub[deltaB.subs++] = subB2;
}
