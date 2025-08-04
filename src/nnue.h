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

struct Accumulator {
        alignas(64) std::array<int16_t, HL_N> white;
        alignas(64) std::array<int16_t, HL_N> black;

        void refresh(Board& board);
        static bool needRefresh(Move kingMove);
        void print();
        // addsub, addsubsub, addaddsubsub
        void quiet(Board& board, Color stm, Square add, PieceType addPT, Square sub, PieceType subPT);
        void capture(Board& board, Color stm, Square add, PieceType addPT, Square sub1, PieceType subPT1, Square sub2,
                     PieceType subPT2);
};

struct NNUE {
        alignas(64) std::array<int16_t, HL_N * 768> H1;
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
