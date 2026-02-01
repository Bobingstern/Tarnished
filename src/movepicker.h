#pragma once

#include "eval.h"
#include "external/chess.hpp"
#include "parameters.h"
#include "search.h"
#include "util.h"

enum class MPStage { TTMOVE, GEN_NOISY, NOISY_GOOD, KILLER, GEN_QUIET, QUIET, BAD_NOISY };
enum MPType: uint8_t {DEFAULT, QSEARCH, PROBCUT = QSEARCH};

inline MPStage operator++(MPStage& stage) {
    stage = static_cast<MPStage>(static_cast<int>(stage) + 1);
    return stage;
}

struct MovePicker {
        Search::ThreadInfo* thread;
        Search::Stack* ss;
        Movelist movesList;
        Movelist badNoises;
        MPStage stage;
        Move ttMove;
        int currMove;
        int seeThreshold;
        MPType type;

        // Threats
        Bitboard pawnThreats;
        Bitboard knightThreats;
        Bitboard bishopThreats;
        Bitboard rookThreats;

        MovePicker(Search::ThreadInfo* T, Search::Stack* ss, uint16_t ttm, MPType type) {
            this->thread = T;
            this->ss = ss;
            this->ttMove = Move(ttm);
            this->type = type;
            this->stage = MPStage::TTMOVE;
            this->currMove = 0;
            this->pawnThreats = ss->threats[0];
            this->knightThreats = ss->threats[1];
            this->bishopThreats = ss->threats[2];
            this->rookThreats = ss->threats[3];
            this->seeThreshold = 0;
        }
        MovePicker(Search::ThreadInfo* T, Search::Stack* ss, uint16_t ttm, MPType type, int thresh) {
            this->thread = T;
            this->ss = ss;
            this->ttMove = Move(ttm);
            this->type = type;
            this->stage = MPStage::TTMOVE;
            this->currMove = 0;
            this->pawnThreats = ss->threats[0];
            this->knightThreats = ss->threats[1];
            this->bishopThreats = ss->threats[2];
            this->rookThreats = ss->threats[3];
            this->seeThreshold = thresh;
        }

        Move nextMove();
        void scoreMoves(Movelist& moves);
        Move selectHighest(Movelist& moves);
};
