#pragma once

#include "external/chess.hpp"
#include "frc.h"
#include "search.h"
#include <bit>
#include <cassert>
#include <cstring>
#include <random>
#include <sstream>
#include <vector>

using namespace chess;

#define DFRC_DATAGEN

constexpr int SOFT_NODE_COUNT = 5000;
constexpr int HARD_NODE_COUNT = 100000;
constexpr int GAMES_BUFFER = 250;
constexpr int DATAGEN_THREADS = 16;
constexpr int DATAGEN_RANDOM_MOVES = 8;

// Yoink from Prelude
template <size_t size> class U4array {
        std::array<uint8_t, size / 2> data;

    public:
        U4array() {
            data.fill(0);
        }

        uint8_t operator[](size_t index) const {
            assert(index < size);
            return (data[index / 2] >> ((index % 2) * 4)) & 0xF;
        }

        void set(int index, uint8_t value) {
            assert(value == (value & 0b1111));
            data[index / 2] |= value << ((index % 2) * 4);
        }
};

// WIP

struct ScoredMove {
        uint16_t move;
        int16_t score;
        ScoredMove(uint16_t m, int16_t s) {
            move = *reinterpret_cast<uint16_t*>(&m);
            score = s;
        }
};

struct MarlinFormat {
        uint64_t occupancy;
        U4array<32> pieces;
        uint8_t epSquare;
        uint8_t halfmove;
        uint16_t fullmove;
        int16_t eval;
        uint8_t wdl;
        uint8_t extra;

        MarlinFormat() = default;
        MarlinFormat(Board& board);
};

struct ViriEntry {
        MarlinFormat header;
        std::vector<ScoredMove> scores;
        ViriEntry(MarlinFormat h, std::vector<ScoredMove> s) {
            header = h;
            scores = s;
        }
};
void makeRandomMove(Board& board);
void makeRandomMove(Board& board, std::mt19937_64& engine);
void startDatagen(size_t tc, bool isDFRC);
uint16_t packMove(Move m);
void writeViriformat(std::ofstream& outFile, ViriEntry& game);
std::string randomDFRC();

static bool matchesToken(std::string line, std::string token) {
    return line.rfind(token, 0) == 0;
}
bool nextToken(std::string* line, std::string* token);
void handleGenfens(Searcher& searcher, std::string params);

