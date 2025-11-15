#pragma once

#include "external/chess.hpp"
#include "parameters.h"
#include <bit>
#include <cassert>
#include <cstring>
#include <sstream>
#include <vector>

using namespace chess;

constexpr int ReLU = 0;
constexpr int CReLU = 1;
constexpr int SCReLU = 2;

constexpr int ACTIVATION = SCReLU;

const bool IS_LITTLE_ENDIAN = true;

// stole from sf
template <typename IntType> inline IntType readLittleEndian(std::istream& stream) {
    IntType result;

    if (IS_LITTLE_ENDIAN)
        stream.read(reinterpret_cast<char*>(&result), sizeof(IntType));
    else {
        std::uint8_t u[sizeof(IntType)];
        std::make_unsigned_t<IntType> v = 0;

        stream.read(reinterpret_cast<char*>(u), sizeof(IntType));
        for (int i = 0; i < sizeof(IntType); ++i)
            v = (v << 8) | u[sizeof(IntType) - i - 1];

        std::memcpy(&result, &v, sizeof(IntType));
    }

    return result;
}

struct BucketCacheEntry {
    std::array<int16_t, HL_N> features;
    std::array<Bitboard, 8> cachedPieces;
    bool isInit;

    BucketCacheEntry() {
        //cachedPieces.fill(0);
        features.fill(0);
        isInit = false;
    }

    void set(Board& board, std::array<int16_t, HL_N>& feats) {
        for (PieceType pt : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, 
                        PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            cachedPieces[int(pt)] = board.pieces(pt);
        }
        
        cachedPieces[6] = board.us(Color::WHITE);
        cachedPieces[7] = board.us(Color::BLACK);

        features = feats;
    }
};

struct InputBucketCache {
    std::array<std::array<std::array<BucketCacheEntry, INPUT_BUCKETS>, 2>, 2> cache;

    InputBucketCache() {
        for (int persp : {0, 1}) {
            for (int mirror : {0, 1}) {
                for (int b = 0; b < INPUT_BUCKETS; b++) {
                    cache[persp][mirror][b] = BucketCacheEntry();
                }
            }
        }
    }
};

struct FeatureDelta {
    int adds = 0;
    int subs = 0;
    std::array<int, 2> toAdd{};
    std::array<int, 2> toSub{};

    FeatureDelta() {
        toAdd.fill(0);
        toSub.fill(0);
    }
    void clear() {
        toAdd.fill(0);
        toSub.fill(0);
        adds = 0;
        subs = 0;
    }
};

struct Accumulator {
        alignas(64) std::array<int16_t, HL_N> white;
        alignas(64) std::array<int16_t, HL_N> black;

        std::array<bool, 2> needsRefresh{};
        std::array<bool, 2> computed{};
        std::array<FeatureDelta, 2> featureDeltas{};

        void refresh(Board& board);
        void refresh(Board& board, Color persp);
        void refresh(Board& board, Color persp, InputBucketCache& bucketCache);

        void applyDelta(Color persp, Accumulator& prev);
        void addSubDelta(Color persp, int addF, int subF);
        void addSubSubDelta(Color persp, int addF, int subF1, int subF2);
        void addAddSubSubDelta(Color persp, int addF1, int addF2, int subF1, int subF2);

        static bool needRefresh(Move kingMove, Color stm);
        static int kingBucket(Square kingSq, Color color);
        void print();

        // addaddaddadd, subsubsubsub
        void refreshAdd4(std::array<int16_t, HL_N>& acc, int add0, int add1, int add2, int add3);
        void refreshSub4(std::array<int16_t, HL_N>& acc, int sub0, int sub1, int sub2, int sub3);
        // addsub, addsubsub, addaddsubsub
        void addPiece(Board& board, Color stm, Square add, PieceType addPT);
        void subPiece(Board& board, Color stm, Square sub, PieceType subPT);

        void addPiece(Board& board, Color stm, Color persp, Square add, PieceType addPT);
        void subPiece(Board& board, Color stm, Color persp, Square sub, PieceType subPT);

        void quiet(Board& board, Color stm, Square add, PieceType addPT, Square sub, PieceType subPT);
        void capture(Board& board, Color stm, Square add, PieceType addPT, Square sub1, PieceType subPT1, Square sub2,
                     PieceType subPT2);
};

struct NNUE {
        alignas(64) std::array<int16_t, HL_N * 768 * INPUT_BUCKETS> H1;
        alignas(64) std::array<int16_t, HL_N> H1Bias;
        alignas(64) std::array<std::array<int16_t, HL_N * 2>, OUTPUT_BUCKETS> OW;
        alignas(64) std::array<int16_t, OUTPUT_BUCKETS> outputBias;

        int16_t ReLU_(int16_t x);
        int16_t CReLU_(int16_t x);
        int32_t SCReLU_(int16_t x);

        static int feature(Color persp, Color color, PieceType piece, Square square, Square king);

        void load(const std::string& file);
        void randomize();

        int32_t optimizedSCReLU(const std::array<int16_t, HL_N>& STM, const std::array<int16_t, HL_N>& OPP, Color col,
                                size_t bucket);
        int inference(Board& board, Accumulator& accumulator);
};

extern NNUE network;