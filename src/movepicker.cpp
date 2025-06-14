#include "external/chess.hpp"
#include "parameters.h"
#include "search.h"
#include "util.h"
#include "movepicker.h"

// Based off Weiss
using enum MPStage;

void MovePicker::scoreMoves(Movelist &moves) {
	for (auto &move : moves) {
		if (stage == GEN_NOISY){
			PieceType to = thread->board.at<PieceType>(move.to());
			if (move.typeOf() == Move::ENPASSANT)
				to = PieceType::PAWN;
			int score = thread->getCapthist(thread->board, move) + MVV_VALUES[to];
			move.setScore(score - 800000 * !SEE(thread->board, move, -PawnValue));
		}
		else {
			move.setScore(thread->getQuietHistory(thread->board, move, ss));
		}
	}
}

Move MovePicker::selectHighest(Movelist &moves) {
	int bestScore = -INFINITE;
	uint32_t bestIndex = currMove;
	for (uint32_t i = currMove; i < moves.size(); i++) {
		if (moves[i].score() > bestScore) {
			bestScore = moves[i].score();
			bestIndex = i;
		}
	}

	std::swap(moves[bestIndex], moves[currMove]);
	return moves[currMove++];
}

Move MovePicker::nextMove() {
	switch (stage) {
		case TTMOVE:
			++stage;
			if (isLegal(thread->board, ttMove) && !(isQS && !thread->board.inCheck())){
				return ttMove;
			}
		case GEN_NOISY:
			movegen::legalmoves<movegen::MoveGenType::CAPTURE>(movesList, thread->board);
			scoreMoves(movesList);
			++stage;

		case NOISY_GOOD:
			while (currMove < movesList.size()) {
				Move move = selectHighest(movesList);
				if (move == ttMove)
					continue;
				return move;
			}
			++stage;

		case KILLER:
			++stage;
			if (ss->killer != ttMove && !isQS && isLegal(thread->board, ss->killer))
				return ss->killer;

		case GEN_QUIET:
			if (thread->board.inCheck() || !isQS) {
				movegen::legalmoves<movegen::MoveGenType::QUIET>(movesList, thread->board);
				scoreMoves(movesList);
			}
			++stage;

		case QUIET:
			while (currMove < movesList.size()) {
				Move move = selectHighest(movesList);
				return move;
			}
			++stage;
			return Move(Move::NO_MOVE);

	}

	if (currMove >= movesList.size()){
		return Move(Move::NO_MOVE);
	}
}