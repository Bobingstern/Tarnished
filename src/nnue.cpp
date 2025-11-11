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

void Accumulator::addPiece(Board& board, Color stm, Color persp, Square add, PieceType addPT) {
    Square kingSq = board.kingSq(persp);
    auto& accPerspective = persp == Color::WHITE ? white : black;
    const int feature = NNUE::feature(persp, stm, addPT, add, kingSq);

#ifndef AUTOVEC
    int16_t* accPtr = accPerspective.data();
    const int16_t* h1Ptr = &network.H1[feature * HL_N];
    nativeVector* accV = reinterpret_cast<nativeVector*>(accPtr);
    const nativeVector* h1V = reinterpret_cast<const nativeVector*>(h1Ptr);

    const size_t VECTOR_SIZE = sizeof(nativeVector) / sizeof(int16_t);
    for (int i = 0; i < HL_N / VECTOR_SIZE; i++) {
        accV[i] = add_epi16(accV[i], h1V[i]);
    }
    
#else
    for (int i = 0; i < HL_N; i++) {
        accPerspective[i] += network.H1[feature * HL_N + i];
    }
#endif
}

void Accumulator::subPiece(Board& board, Color stm, Color persp, Square sub, PieceType subPT) {
    Square kingSq = board.kingSq(persp);
    auto& accPerspective = persp == Color::WHITE ? white : black;

    const int feature = NNUE::feature(persp, stm, subPT, sub, kingSq);

#ifndef AUTOVEC
    int16_t* accPtr = accPerspective.data();
    const int16_t* h1Ptr = &network.H1[feature * HL_N];
    nativeVector* accV = reinterpret_cast<nativeVector*>(accPtr);
    const nativeVector* h1V = reinterpret_cast<const nativeVector*>(h1Ptr);

    const size_t VECTOR_SIZE = sizeof(nativeVector) / sizeof(int16_t);
    for (int i = 0; i < HL_N / VECTOR_SIZE; i++) {
        accV[i] = sub_epi16(accV[i], h1V[i]);
    }
    
#else
    for (int i = 0; i < HL_N; i++) {
        accPerspective[i] += network.H1[feature * HL_N + i];
    }
#endif
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

#ifndef AUTOVEC
    nativeVector* wPtr = reinterpret_cast<nativeVector*>(white.data());
    nativeVector* bPtr = reinterpret_cast<nativeVector*>(black.data());

    const nativeVector* addWPtr = reinterpret_cast<const nativeVector*>(&network.H1[addW * HL_N]);
    const nativeVector* addBPtr = reinterpret_cast<const nativeVector*>(&network.H1[addB * HL_N]);
    const nativeVector* subWPtr = reinterpret_cast<const nativeVector*>(&network.H1[subW * HL_N]);
    const nativeVector* subBPtr = reinterpret_cast<const nativeVector*>(&network.H1[subB * HL_N]);

    const size_t VECTOR_SIZE = sizeof(nativeVector) / sizeof(int16_t);
    for (int i = 0; i < HL_N / VECTOR_SIZE; i++) {
        nativeVector w  = load_epi16(&wPtr[i]);
        nativeVector b  = load_epi16(&bPtr[i]);
        nativeVector aw = load_epi16(&addWPtr[i]);
        nativeVector ab = load_epi16(&addBPtr[i]);
        nativeVector sw = load_epi16(&subWPtr[i]);
        nativeVector sb = load_epi16(&subBPtr[i]);

        wPtr[i] = add_epi16(wPtr[i], sub_epi16(aw, sw));
        bPtr[i] = add_epi16(bPtr[i], sub_epi16(ab, sb));
    }
#else
    for (int i = 0; i < HL_N; i++) {
        white[i] += network.H1[addW * HL_N + i] - network.H1[subW * HL_N + i];
        black[i] += network.H1[addB * HL_N + i] - network.H1[subB * HL_N + i];
    }
#endif
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

#ifndef AUTOVEC
    nativeVector*       wPtr   = reinterpret_cast<nativeVector*>(white.data());
    nativeVector*       bPtr   = reinterpret_cast<nativeVector*>(black.data());
    const nativeVector* addWPtr = reinterpret_cast<const nativeVector*>(&network.H1[addW * HL_N]);
    const nativeVector* addBPtr = reinterpret_cast<const nativeVector*>(&network.H1[addB * HL_N]);
    const nativeVector* subW1Ptr = reinterpret_cast<const nativeVector*>(&network.H1[subW1 * HL_N]);
    const nativeVector* subB1Ptr = reinterpret_cast<const nativeVector*>(&network.H1[subB1 * HL_N]);
    const nativeVector* subW2Ptr = reinterpret_cast<const nativeVector*>(&network.H1[subW2 * HL_N]);
    const nativeVector* subB2Ptr = reinterpret_cast<const nativeVector*>(&network.H1[subB2 * HL_N]);

    const size_t VECTOR_SIZE = sizeof(nativeVector) / sizeof(int16_t);
    for (int i = 0; i < HL_N / VECTOR_SIZE; i++) {
        nativeVector aw  = addWPtr[i];
        nativeVector ab  = addBPtr[i];
        nativeVector sw1 = subW1Ptr[i];
        nativeVector sb1 = subB1Ptr[i];
        nativeVector sw2 = subW2Ptr[i];
        nativeVector sb2 = subB2Ptr[i];

        // white[i] += addW - subW1 - subW2
        wPtr[i] = add_epi16(wPtr[i], sub_epi16(sub_epi16(aw, sw1), sw2));

        // black[i] += addB - subB1 - subB2
        bPtr[i] = add_epi16(bPtr[i], sub_epi16(sub_epi16(ab, sb1), sb2));
    }
#else
    for (int i = 0; i < HL_N; i++) {
        white[i] += network.H1[addW * HL_N + i] - network.H1[subW1 * HL_N + i] -
                    network.H1[subW2 * HL_N + i];
        black[i] += network.H1[addB * HL_N + i] - network.H1[subB1 * HL_N + i] -
                    network.H1[subB2 * HL_N + i];
    }
#endif
}
