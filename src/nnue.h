#pragma once

#include "external/chess.hpp"
#include "parameters.h"
#include <bit>
#include <cassert>
#include <cstring>
#include <sstream>
#include <vector>

using namespace chess;

struct UnquantisedNetwork {
    float Factoriser[L1_SIZE * 768];
    float FTWeights[INPUT_BUCKETS * L1_SIZE * 768];
    float FTBiases[L1_SIZE];
    float L1Weights[L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float L1Biases[OUTPUT_BUCKETS][L2_SIZE];
    float L2Weights[L2_SIZE * 2][OUTPUT_BUCKETS][L3_SIZE];
    float L2Biases[OUTPUT_BUCKETS][L3_SIZE];
    float L3Weights[L3_SIZE][OUTPUT_BUCKETS];
    float L3Biases[OUTPUT_BUCKETS];
};

struct QuantisedNetwork {
    int16_t FTWeights[INPUT_BUCKETS * L1_SIZE * 768];
    int16_t FTBiases [L1_SIZE];
    int8_t L1Weights[L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float L1Biases[OUTPUT_BUCKETS][L2_SIZE];
    float L2Weights[L2_SIZE * 2][OUTPUT_BUCKETS][L3_SIZE];
    float L2Biases[OUTPUT_BUCKETS][L3_SIZE];
    float L3Weights[L3_SIZE][OUTPUT_BUCKETS];
    float L3Biases[OUTPUT_BUCKETS];
};

struct Network {
    alignas(64) int16_t FTWeights[INPUT_BUCKETS * L1_SIZE * 768];
    alignas(64) int16_t FTBiases[L1_SIZE];
    alignas(64) int8_t L1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
    alignas(64) float L1Biases[OUTPUT_BUCKETS][L2_SIZE];
    alignas(64) float L2Weights[OUTPUT_BUCKETS][L2_SIZE * 2 * L3_SIZE];
    alignas(64) float L2Biases[OUTPUT_BUCKETS][L3_SIZE];
    alignas(64) float L3Weights[OUTPUT_BUCKETS][L3_SIZE];
    alignas(64) float L3Biases[OUTPUT_BUCKETS];
};

void quantise_raw();

struct BucketCacheEntry {
    std::array<int16_t, L1_SIZE> features;
    std::array<Bitboard, 8> cachedPieces;
    bool isInit;

    BucketCacheEntry() {
        //cachedPieces.fill(0);
        features.fill(0);
        isInit = false;
    }

    void set(Board& board, std::array<int16_t, L1_SIZE>& feats) {
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
        alignas(64) std::array<int16_t, L1_SIZE> white;
        alignas(64) std::array<int16_t, L1_SIZE> black;

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
        void refreshAdd4(std::array<int16_t, L1_SIZE>& acc, int add0, int add1, int add2, int add3);
        void refreshSub4(std::array<int16_t, L1_SIZE>& acc, int sub0, int sub1, int sub2, int sub3);
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

        static int feature(Color persp, Color color, PieceType piece, Square square, Square king);
        int inference(Board& board, Accumulator& accumulator);

        void activateL1(Accumulator& acc, Color stm, uint8_t* output);
        void forwardL1(const uint8_t* inputs, const int8_t* weights, const float* biases, float* output);
        void forwardL2(const float* inputs, const float* weights, const float* biases, float* output);
        void forwardL3(const float* inputs, const float* weights, const float bias, float& output);
};

extern NNUE network;
extern QuantisedNetwork quantisedNet;
extern UnquantisedNetwork unquantisedNet;
extern const Network* permutedNet;