#include "nnue.h"
#include "search.h"
#include "simd.h"

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


    nativeVector eval{};

    for (int c = 0; c <= 1; ++c) {
        const nativeVector* __restrict__ acc =
            reinterpret_cast<const nativeVector*>(
                (c == 0 ? STM.data() : OPP.data())
            );

        const nativeVector* __restrict__ w =
            reinterpret_cast<const nativeVector*>(
                OW[bucket].data() + c * HL_N / 2
            );
        for (int i = 0; i < (HL_N / VECTOR_SIZE) / 2; ++i) {
            // compute the clipped ReLU of the inputs, v
            const nativeVector clamped = min_epi16(max_epi16(acc[i], VEC_ZERO), VEC_QA);
            const nativeVector clamped2 = min_epi16(max_epi16(acc[i + (HL_N / VECTOR_SIZE) / 2], VEC_ZERO), VEC_QA);
            // Pairwise Mult
            const nativeVector activated = madd_epi16(mullo_epi16(clamped, w[i]), clamped2);

            eval = add_epi32(eval, activated);
        }
    }
    return reduce_epi32(eval);
}

#else

int32_t NNUE::optimizedSCReLU(const std::array<int16_t, HL_N>& STM,
                              const std::array<int16_t, HL_N>& OPP, Color col,
                              size_t bucket) {
    int32_t eval = 0;
    for (int i = 0; i < HL_N / 2; i++) {
        eval += CReLU_(STM[i]) * CReLU_(STM[i + HL_N / 2]) * OW[bucket][i];
        eval += CReLU_(OPP[i]) * CReLU_(OPP[i + HL_N / 2]) * OW[bucket][i + HL_N / 2];
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
        for (int j = 0; j < HL_N; j++)
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
    eval = optimizedSCReLU(accumulatorSTM, accumulatorOPP, stm, outputBucket);
    eval /= QA;
    

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

    needsRefresh[int(persp)] = false;
    computed[int(persp)] = true;

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

    needsRefresh[int(persp)] = false;
    computed[int(persp)] = true;

    BucketCacheEntry& cache = bucketCache.cache[int(persp)][kingSq.file() >= File::FILE_E][kingBucket(kingSq, persp)];

    if (!cache.isInit) {
        cache.features = network.H1Bias;
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
            accPerspective[i] += network.H1[adds[addIndex - 1] * HL_N + i];
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
            accPerspective[i] -= network.H1[subs[subIndex - 1] * HL_N + i];
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
        acc[i] += network.H1[add0 * HL_N + i];
        acc[i] += network.H1[add1 * HL_N + i];
        acc[i] += network.H1[add2 * HL_N + i];
        acc[i] += network.H1[add3 * HL_N + i];
    }
}
void Accumulator::refreshSub4(std::array<int16_t, HL_N>& acc, int sub0, int sub1, int sub2, int sub3) {
    for (int i = 0; i < HL_N; i++) {
        acc[i] -= network.H1[sub0 * HL_N + i];
        acc[i] -= network.H1[sub1 * HL_N + i];
        acc[i] -= network.H1[sub2 * HL_N + i];
        acc[i] -= network.H1[sub3 * HL_N + i];
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
        accPerspective[i] += network.H1[addF * HL_N + i];
        accPerspective[i] -= network.H1[subF * HL_N + i];
    }
}
void Accumulator::addSubSubDelta(Color persp, int addF, int subF1, int subF2) {
    auto& accPerspective = persp == Color::WHITE ? white : black;
    for (int i = 0; i < HL_N; i++) {
        accPerspective[i] += network.H1[addF * HL_N + i];
        accPerspective[i] -= network.H1[subF1 * HL_N + i];
        accPerspective[i] -= network.H1[subF2 * HL_N + i];
    }
}
void Accumulator::addAddSubSubDelta(Color persp, int addF1, int addF2, int subF1, int subF2) {
    auto& accPerspective = persp == Color::WHITE ? white : black;
    for (int i = 0; i < HL_N; i++) {
        accPerspective[i] += network.H1[addF1 * HL_N + i];
        accPerspective[i] += network.H1[addF2 * HL_N + i];
        accPerspective[i] -= network.H1[subF1 * HL_N + i];
        accPerspective[i] -= network.H1[subF2 * HL_N + i];
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
