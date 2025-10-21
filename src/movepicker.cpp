#include "movepicker.h"
#include "external/chess.hpp"
#include "parameters.h"
#include "search.h"
#include "util.h"

// Based off Weiss
using enum MPStage;

void MovePicker::scoreMoves(Movelist& moves) {
    for (auto& move : moves) {
        if (stage == GEN_NOISY || move.typeOf() == Move::CASTLING) {
            PieceType to = thread->board.at<PieceType>(move.to());
            if (move.typeOf() == Move::ENPASSANT)
                to = PieceType::PAWN;
            int score =
                thread->getCapthist(thread->board, move, ss) + MVV_VALUES[to];
            move.setScore(score);
        } else {
            PieceType pt = thread->board.at<PieceType>(move.from());
            Bitboard fromBB = Bitboard::fromSquare(move.from());
            Bitboard toBB = Bitboard::fromSquare(move.to());

            int score = thread->getQuietHistory(thread->board, move, ss);
            // Penalize if we're moving into a threat and vice versa
            if (pt == PieceType::QUEEN) {
                // Everything threatens a queen
                Bitboard threats = pawnThreats | knightThreats | bishopThreats | rookThreats;
                score += (threats & fromBB).empty() ? 0 : 12228;
                score -= (threats & toBB).empty() ? 0 : 11264;
            }
            else if (pt == PieceType::ROOK) {
                // P, N, B
                Bitboard threats = pawnThreats | knightThreats | bishopThreats;
                score += (threats & fromBB).empty() ? 0 : 10240;
                score -= (threats & toBB).empty() ? 0 : 9216;
            }
            else if (pt == PieceType::BISHOP || pt == PieceType::KNIGHT) {
                // P
                Bitboard threats = pawnThreats;
                score += (threats & fromBB).empty() ? 0 : 8192;
                score -= (threats & toBB).empty() ? 0 : 7168;
            }

            move.setScore(score);
        }
    }
}

Move MovePicker::selectHighest(Movelist& moves) {
    int bestScore = moves[currMove].score();
    uint32_t bestIndex = currMove;
    for (uint32_t i = currMove; i < moves.size(); i++) {
        if (moves[i].score() > bestScore) {
            bestScore = moves[i].score();
            bestIndex = i;
        }
    }

    std::iter_swap(moves.begin() + bestIndex, moves.begin() + currMove);
    return moves[currMove++];
}

Move MovePicker::nextMove() {
    switch (stage) {
        case TTMOVE:
            ++stage;
            // Only return ttMove if in QS if we're in check or if its a capture
            // If in noisy good only, only return noisy tt
            if (isLegal(thread->board, ttMove)) {
                if (thread->board.isCapture(ttMove)) {
                    return ttMove;
                }
                if ( (!isQS || thread->board.inCheck()) && !goodNoisyOnly ) {
                    return ttMove;
            }
                
            }
        case GEN_NOISY:
            movegen::legalmoves<movegen::MoveGenType::CAPTURE>(movesList,
                                                               thread->board);
            scoreMoves(movesList);
            ++stage;

        case NOISY_GOOD:
            while (currMove < movesList.size()) {
                Move move = selectHighest(movesList);
                if (move == ttMove) {
                    continue;
                }
                int margin = goodNoisyOnly ? seeMargin : -move.score() / 4 + 15;
                if (!SEE(thread->board, move, margin))
                    badNoises.add(move);
                else
                    return move;
            }
            if (goodNoisyOnly) {
                stage = MPStage::END;
                return Move(Move::NO_MOVE);
            }
            ++stage;

        case KILLER:
            ++stage;
            if (ss->killer != ttMove && !isQS &&
                isLegal(thread->board, ss->killer))
                return ss->killer;

        case GEN_QUIET:
            movesList.clear();
            currMove = 0;
            if (thread->board.inCheck() || !isQS) {
                movegen::legalmoves<movegen::MoveGenType::QUIET>(movesList,
                                                                 thread->board);
                scoreMoves(movesList);
            }
            ++stage;

        case QUIET:
            while (currMove < movesList.size()) {
                Move move = selectHighest(movesList);
                if (move == ttMove)
                    continue;
                return move;
            }
            currMove = 0;
            ++stage;

        case BAD_NOISY:
            while (currMove < badNoises.size()) {
                Move move = selectHighest(badNoises);
                if (move == ttMove)
                    continue;
                return move;
            }
            ++stage;
            
        case END:
            return Move(Move::NO_MOVE);
    }

    if (currMove >= movesList.size()) {
        return Move(Move::NO_MOVE);
    }
}