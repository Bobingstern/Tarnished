#pragma once

#include "external/chess.hpp"
#include "search.h"
#include <bit>
#include <vector>
#include <sstream>
#include <cassert>
#include <cstring>

using namespace chess;
constexpr int SOFT_NODE_COUNT = 5000;
constexpr int HARD_NODE_COUNT = 100000;
constexpr int GAMES_BUFFER = 250;
constexpr int DATAGEN_THREADS = 16;
constexpr int DATAGEN_RANDOM_MOVES = 8;

// Yoink from Prelude
template<size_t size>
class U4array {
    std::array<uint8_t, size / 2> data;

public:
    U4array() {
        data.fill(0);
    }

    uint8_t operator[](size_t index) const {
        assert(index < size);
        return (data[index / 2] >> ((index % 2) * 4)) & 0xF;
        // if (index % 2 == 0) return data[index / 2] & 0b1111;
        // return data[index / 2] >> 4;
    }

    void set(int index, uint8_t value) {
        assert(value == (value & 0b1111));
        data[index/2] |= value << ((index % 2) * 4);
        // if (index % 2 == 0) {
        //     data[index / 2] &= 0b11110000;
        //     data[index / 2] |= value;
        // }
        // else {
        //     data[index / 2] &= 0b1111;
        //     data[index / 2] |= value << 4;
        // }
    }
};


// WIP

struct ScoredMove {
	uint16_t move;
	int16_t score;
	ScoredMove(uint16_t m, int16_t s){
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
	MarlinFormat(Board &board);
};


struct ViriEntry {
    MarlinFormat header;
    std::vector<ScoredMove> scores;
    ViriEntry(MarlinFormat h, std::vector<ScoredMove> s){
        header = h;
        scores = s;
    }
};
void startDatagen(size_t tc);
uint16_t packMove(Move m);
void writeViriformat(std::ofstream &outFile, ViriEntry &game);

#ifdef STORE_LMR_DATA
void lmrDatagen();
#endif