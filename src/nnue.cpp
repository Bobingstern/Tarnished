#include "nnue.h"
#include "search.h"
#include "parameters.h"

#include <algorithm>
#include <fstream>
#include <random>

QuantisedNetwork quantisedNet;
UnquantisedNetwork unquantisedNet;
const Network* permutedNet;

// Multilayer inference heavily based off Alexandria
// Thanks Zuppa and CJ
void quantise_raw() {
    // https://github.com/PGG106/Alexandria/blob/fdf5dbd744ea601f9d7dbedcc9dfe1c391aba37c/src/nnue.cpp#L32
    std::ifstream stream{"raw.bin", std::ios::binary};

    stream.read(reinterpret_cast<char *>(&unquantisedNet), sizeof(UnquantisedNetwork));

    for (int bucket = 0; bucket < INPUT_BUCKETS; ++bucket) {
        int bucket_offset = bucket * (768 * L1_SIZE);

        for (int i = 0; i < 768 * L1_SIZE; ++i) {
            float w = unquantisedNet.FTWeights[bucket_offset + i] + unquantisedNet.Factoriser[i];

            quantisedNet.FTWeights[bucket_offset + i] = static_cast<int16_t>(std::round(w * QA));
        }
    }

    for (int i = 0; i < L1_SIZE; ++i)
        quantisedNet.FTBiases[i] = static_cast<int16_t>(std::round(unquantisedNet.FTBiases[i] * QA));

    for (int bucket = 0; bucket < OUTPUT_BUCKETS; ++bucket) {
        for (int i = 0; i < L1_SIZE; ++i)
            for (int j = 0; j < L2_SIZE; ++j)
                quantisedNet.L1Weights[i][bucket][j] = 
                        static_cast<int8_t>(std::round(unquantisedNet.L1Weights[i][bucket][j] * 64));

        for (int i = 0; i < L2_SIZE; ++i) {
            quantisedNet.L1Biases[bucket][i] = unquantisedNet.L1Biases[bucket][i];
        }

        for (int i = 0; i < L2_SIZE * 2; ++i)
            for (int j = 0; j < L3_SIZE; ++j)
                quantisedNet.L2Weights[i][bucket][j] = unquantisedNet.L2Weights[i][bucket][j];

        for (int i = 0; i < L3_SIZE; ++i)
            quantisedNet.L2Biases[bucket][i] = unquantisedNet.L2Biases[bucket][i];

        for (int i = 0; i < L3_SIZE; ++i)
            quantisedNet.L3Weights[i][bucket] = unquantisedNet.L3Weights[i][bucket];

        quantisedNet.L3Biases[bucket] = unquantisedNet.L3Biases[bucket];
    }

    std::ofstream out{"quantised.bin", std::ios::binary};
    out.write(reinterpret_cast<const char *>(&quantisedNet), sizeof(QuantisedNetwork));
    std::cout << "Successfully Quantised" << std::endl;
}


int NNUE::feature(Color persp, Color color, PieceType p, Square sq, Square king) {
    int ci = persp == color ? 0 : 1;
    int sqi = persp == Color::BLACK ? sq.flip().index() : sq.index();
    if (king.file() >= File::FILE_E && HORIZONTAL_MIRROR)
        sqi ^= 7;
    return Accumulator::kingBucket(king, persp) * 768 + ci * 64 * 6 + int(p) * 64 + sqi; // Index of the feature
}


void NNUE::activateL1(Accumulator& acc, Color col, uint8_t* output) {
    const std::array<int16_t, L1_SIZE>& stm =
        col == Color::WHITE ? acc.white : acc.black;
    const std::array<int16_t, L1_SIZE>& opp =
        col == Color::BLACK ? acc.white : acc.black;

#ifndef AUTOVEC
    const vepi16 vecOne = set1_epi16(QA);
    const vepi16 vecZero = set1_epi16(0);
    int offset = 0;
    for (auto& side : {stm, opp}) {
        for (int i = 0; i < L1_SIZE / 2; i += 2 * FT_CHUNK_SIZE) {
            const vepi16 i0a = load_epi16(reinterpret_cast<const vepi16*>(&side[i]));
            const vepi16 i0b = load_epi16(reinterpret_cast<const vepi16*>(&side[i + FT_CHUNK_SIZE]));
            const vepi16 i1a = load_epi16(reinterpret_cast<const vepi16*>(&side[i + L1_SIZE / 2]));
            const vepi16 i1b = load_epi16(reinterpret_cast<const vepi16*>(&side[i + FT_CHUNK_SIZE + L1_SIZE / 2]));

            const vepi16 c0a = min_epi16(max_epi16(i0a, vecZero), vecOne);
            const vepi16 c0b = min_epi16(max_epi16(i0b, vecZero), vecOne);
            const vepi16 c1a = min_epi16(i1a, vecOne);
            const vepi16 c1b = min_epi16(i1b, vecOne);

            const vepi8  product = packus_epi16(mulhi_epi16(slli_epi16(c0a, 16 - FT_SHIFT), c1a), 
                                                    mulhi_epi16(slli_epi16(c0b, 16 - FT_SHIFT), c1b));
            store_epi16(reinterpret_cast<vepi8*>(&output[i + offset]), product);
        }
        offset += L1_SIZE / 2;
    }

#else

    for (int i = 0; i < L1_SIZE / 2; i++) {
        int16_t c0 = std::clamp<int16_t>(stm[i], 0, QA);
        int16_t c1 = std::clamp<int16_t>(stm[i + L1_SIZE / 2], 0, QA);
        output[i] = static_cast<uint8_t>(c0 * c1 >> FT_SHIFT);

        c0 = std::clamp<int16_t>(opp[i], 0, QA);
        c1 = std::clamp<int16_t>(opp[i + L1_SIZE / 2], 0, QA);
        output[i + L1_SIZE / 2] = static_cast<uint8_t>(c0 * c1 >> FT_SHIFT);
    }

#endif
}

void NNUE::forwardL1(const uint8_t* inputs, const int8_t* weights, const float* biases, float* output) {

#ifndef AUTOVEC
    vepi32 sums[L2_SIZE / L2_CHUNK_SIZE] = {0};
    const int32_t *inputs32 = reinterpret_cast<const int32_t*>(inputs);
    int i = 0;
    for (; i + 1 < L1_SIZE / L1_CHUNK_PER_32; i += 2){
        const vepi32 ia = set1_epi32(inputs32[i]);
        const vepi32 ib = set1_epi32(inputs32[i + 1]);
        const vepi8 *wa = reinterpret_cast<const vepi8*>(&weights[i * L1_CHUNK_PER_32 * L2_SIZE]);
        const vepi8 *wb = reinterpret_cast<const vepi8*>(&weights[(i + 1) * L1_CHUNK_PER_32 * L2_SIZE]);
        for (int j = 0; j < L2_SIZE / L2_CHUNK_SIZE; ++j)
            sums[j] = dpbusdx2_epi32(sums[j], ia, wa[j], ib, wb[j]);
    }

    for (; i < L1_SIZE / L1_CHUNK_PER_32; ++i) {
        const vepi32 input32 = set1_epi32(inputs32[i]);
        const vepi8 *weight = reinterpret_cast<const vepi8*>(&weights[i * L1_CHUNK_PER_32 * L2_SIZE]);
        for (int j = 0; j < L2_SIZE / L2_CHUNK_SIZE; ++j)
            sums[j] = dpbusd_epi32(sums[j], input32, weight[j]);
    }

    for (i = 0; i < L2_SIZE / L2_CHUNK_SIZE; ++i) {
        const vps32 sumPs = mul_add_ps(cvtepi32_ps(sums[i]), 
                                        set1_ps(L1_MUL), 
                                        load_ps(&biases[i * L2_CHUNK_SIZE]));
        const vps32 c = min_ps(max_ps(sumPs, zero_ps()), set1_ps(1.0f));

        const vps32 sqc = min_ps(max_ps(mul_ps(sumPs, sumPs), zero_ps()), set1_ps(1.0f));

        store_ps(&output[i * L2_CHUNK_SIZE], c);
        store_ps(&output[L2_SIZE + i * L2_CHUNK_SIZE], sqc);
    }
#else
    int sums[L2_SIZE] = {0};
    for (int i = 0; i < L1_SIZE; ++i) {
        for (int j = 0; j < L2_SIZE; ++j) {
            sums[j] += static_cast<int32_t>(inputs[i] * weights[j * L1_SIZE + i]);
        }
    }

    for (int i = 0; i < L2_SIZE; ++i) {
        const float z = float(sums[i]) * L1_MUL + biases[i];
        output[i] = std::clamp(z, 0.0f, 1.0f);
        output[i + L2_SIZE] = std::clamp(z * z, 0.0f, 1.0f);
    }
#endif
}

void NNUE::forwardL2(const float* inputs, const float* weights, const float* biases, float* output) {

#ifndef AUTOVEC
    vps32 sumVecs[L3_SIZE / L3_CHUNK_SIZE];

    for (int i = 0; i < L3_SIZE / L3_CHUNK_SIZE; ++i)
        sumVecs[i] = load_ps(&biases[i * L3_CHUNK_SIZE]);

    for (int i = 0; i < L2_SIZE * 2; ++i) {
        const vps32 inputVec = set1_ps(inputs[i]);
        const vps32 *weight = reinterpret_cast<const vps32*>(&weights[i * L3_SIZE]);
        for (int j = 0; j < L3_SIZE / L3_CHUNK_SIZE; ++j)
            sumVecs[j] = mul_add_ps(inputVec, weight[j], sumVecs[j]);
    }

    for (int i = 0; i < L3_SIZE / L3_CHUNK_SIZE; ++i) {
        const vps32 c = min_ps(max_ps(sumVecs[i], zero_ps()), set1_ps(1.0f));
        store_ps(&output[i * L3_CHUNK_SIZE],  mul_ps(c, c));
    }
#else
    float sums[L3_SIZE];

    for (int i = 0; i < L3_SIZE; ++i)
        sums[i] = biases[i];

    for (int i = 0; i < L2_SIZE * 2; ++i) {
        const float *weight = &weights[i * L3_SIZE];
        for (int out = 0; out < L3_SIZE; ++out) {
            sums[out] += inputs[i] * weight[out];
        }
    }

    for (int i = 0; i < L3_SIZE; ++i) {
        float c = std::clamp(sums[i], 0.0f, 1.0f);
        output[i] = c * c;
    }
#endif
}

void NNUE::forwardL3(const float* inputs, const float* weights, const float bias, float& output) {
    constexpr int chunk = 512 / 32;
    
#ifndef AUTOVEC
    constexpr int numSums = chunk / (sizeof(vps32) / sizeof(float));
    vps32 sumVecs[numSums] = {0};
    for (int i = 0; i < L3_SIZE / L3_CHUNK_SIZE; ++i) {
        const vps32 weightVec = load_ps(&weights[i * L3_CHUNK_SIZE]);
        const vps32 inputsVec = load_ps(&inputs[i * L3_CHUNK_SIZE]);
        sumVecs[i % numSums] = mul_add_ps(inputsVec, weightVec, sumVecs[i % numSums]);
    }
    output = reduce_add_ps(sumVecs) + bias;
#else
    constexpr int numSums = chunk;
    float sums[numSums] = {0};

    for (int i = 0; i < L3_SIZE; ++i) {
        sums[i % numSums] += inputs[i] * weights[i];
    }
    output = reduce_add(sums, numSums) + bias;
#endif
}

int NNUE::inference(Board& board, Accumulator& accumulator) {

    Color stm = board.sideToMove();
    const size_t outputBucket = (board.occ().count() - 2) / (32 / OUTPUT_BUCKETS);
    alignas (64) uint8_t FTOutputs[L1_SIZE];
    alignas (64) float L1Outputs[L2_SIZE * 2];
    alignas (64) float L2Outputs[L3_SIZE];
    float output;

    activateL1(accumulator, stm, FTOutputs);
    forwardL1(FTOutputs, permutedNet->L1Weights[outputBucket], permutedNet->L1Biases[outputBucket], L1Outputs);
    forwardL2(L1Outputs, permutedNet->L2Weights[outputBucket], permutedNet->L2Biases[outputBucket], L2Outputs);
    forwardL3(L2Outputs, permutedNet->L3Weights[outputBucket], permutedNet->L3Biases[outputBucket], output);
    return output * NNUE_SCALE;
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

        for (int i = 0; i < L1_SIZE; i++) {
            // Do the matrix mutliply for the next layer
            accPerspective[i] += permutedNet->FTWeights[feature * L1_SIZE + i];
        }
    }

    while (blackBB) {
        Square sq = blackBB.pop();

        // Black features for both perspectives
        int feature = NNUE::feature(persp, Color::BLACK,
                               board.at<PieceType>(sq), sq, kingSq);
        
        for (int i = 0; i < L1_SIZE; i++) {
            // Do the matrix mutliply for the next layer
            accPerspective[i] += permutedNet->FTWeights[feature * L1_SIZE + i];
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
        for (int i = 0; i < L1_SIZE; i++) {
            accPerspective[i] += permutedNet->FTWeights[adds[addIndex - 1] * L1_SIZE + i];
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
        for (int i = 0; i < L1_SIZE; i++) {
            accPerspective[i] -= permutedNet->FTWeights[subs[subIndex - 1] * L1_SIZE + i];
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
    for (int i = 0; i < L1_SIZE; i++) {
        std::cout << "White: " << white[i] << " Black: " << black[i]
                  << std::endl;
    }
}


void Accumulator::refreshAdd4(std::array<int16_t, L1_SIZE>& acc, int add0, int add1, int add2, int add3) {
    for (int i = 0; i < L1_SIZE; i++) {
        acc[i] += permutedNet->FTWeights[add0 * L1_SIZE + i];
        acc[i] += permutedNet->FTWeights[add1 * L1_SIZE + i];
        acc[i] += permutedNet->FTWeights[add2 * L1_SIZE + i];
        acc[i] += permutedNet->FTWeights[add3 * L1_SIZE + i];
    }
}
void Accumulator::refreshSub4(std::array<int16_t, L1_SIZE>& acc, int sub0, int sub1, int sub2, int sub3) {
    for (int i = 0; i < L1_SIZE; i++) {
        acc[i] -= permutedNet->FTWeights[sub0 * L1_SIZE + i];
        acc[i] -= permutedNet->FTWeights[sub1 * L1_SIZE + i];
        acc[i] -= permutedNet->FTWeights[sub2 * L1_SIZE + i];
        acc[i] -= permutedNet->FTWeights[sub3 * L1_SIZE + i];
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
    for (int i = 0; i < L1_SIZE; i++) {
        accPerspective[i] += permutedNet->FTWeights[addF * L1_SIZE + i];
        accPerspective[i] -= permutedNet->FTWeights[subF * L1_SIZE + i];
    }
}
void Accumulator::addSubSubDelta(Color persp, int addF, int subF1, int subF2) {
    auto& accPerspective = persp == Color::WHITE ? white : black;
    for (int i = 0; i < L1_SIZE; i++) {
        accPerspective[i] += permutedNet->FTWeights[addF * L1_SIZE + i];
        accPerspective[i] -= permutedNet->FTWeights[subF1 * L1_SIZE + i];
        accPerspective[i] -= permutedNet->FTWeights[subF2 * L1_SIZE + i];
    }
}
void Accumulator::addAddSubSubDelta(Color persp, int addF1, int addF2, int subF1, int subF2) {
    auto& accPerspective = persp == Color::WHITE ? white : black;
    for (int i = 0; i < L1_SIZE; i++) {
        accPerspective[i] += permutedNet->FTWeights[addF1 * L1_SIZE + i];
        accPerspective[i] += permutedNet->FTWeights[addF2 * L1_SIZE + i];
        accPerspective[i] -= permutedNet->FTWeights[subF1 * L1_SIZE + i];
        accPerspective[i] -= permutedNet->FTWeights[subF2 * L1_SIZE + i];
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
