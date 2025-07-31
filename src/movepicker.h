#pragma once

#include "eval.h"
#include "external/chess.hpp"
#include "parameters.h"
#include "search.h"
#include "util.h"

enum class MPStage { TTMOVE, GEN_NOISY, NOISY_GOOD, KILLER, GEN_QUIET, QUIET, BAD_NOISY };

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
        bool isQS;

        // Threats
        Bitboard pawnThreats;
        Bitboard knightThreats;
        Bitboard bishopThreats;
        Bitboard rookThreats;

        MovePicker(Search::ThreadInfo* T, Search::Stack* ss, uint16_t ttm, bool qs) {
            this->thread = T;
            this->ss = ss;
            this->ttMove = Move(ttm);
            isQS = qs;
            stage = MPStage::TTMOVE;
            currMove = 0;
            pawnThreats = ss->threats[0];
            knightThreats = ss->threats[1];
            bishopThreats = ss->threats[2];
            rookThreats = ss->threats[3];
        }

        Move nextMove();
        void scoreMoves(Movelist& moves);
        Move selectHighest(Movelist& moves);
};
