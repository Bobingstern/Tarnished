#include "search.h"
#include "searcher.h"
#include "eval.h"
#include "tt.h"
#include "util.h"
#include "parameters.h"

#include <algorithm>
#include <random>

using namespace chess;

// Accumulator wrappers
// Update the accumulators incrementally and track the
// pawn zobrist key for correction history (stored in the search stack)
// The incremental hash logic is terrible and code is ugly. I will refactor into an add piece and remove piece eventually

void MakeMove(Board &board, Accumulator &acc, Move move, Search::Stack *ss){
	PieceType to = board.at<PieceType>(move.to());
	PieceType from = board.at<PieceType>(move.from());
	Color stm = board.sideToMove();

	// Hash key incremental updates
	(ss+1)->pawnKey = ss->pawnKey;
	(ss+1)->majorKey = ss->majorKey;
	(ss+1)->minorKey = ss->minorKey;
	(ss+1)->nonPawnKey[0] = ss->nonPawnKey[0];
	(ss+1)->nonPawnKey[1] = ss->nonPawnKey[1];
	if (move == Move::NULL_MOVE){
		board.makeNullMove();
		return;
	}
	
	if (from == PieceType::PAWN){
		// Update pawn zobrist key
		// Remove from sq pawn
		(ss+1)->pawnKey ^= Zobrist::piece(board.at(move.from()), move.from());
		if (move.typeOf() == Move::ENPASSANT){
			// Remove en passant pawn
			(ss+1)->pawnKey ^= Zobrist::piece(Piece(PieceType::PAWN, ~stm), move.to().ep_square());
		}
		else if (to == PieceType::PAWN){
			// Remove to sq pawn
			(ss+1)->pawnKey ^= Zobrist::piece(board.at(move.to()), move.to());
		}
		else if (to != PieceType::NONE) {
			(ss+1)->nonPawnKey[~stm] ^= Zobrist::piece(board.at(move.to()), move.to());
			if (isMajor(to))
				(ss+1)->majorKey ^= Zobrist::piece(board.at(move.to()), move.to());
			if (isMinor(to))
				(ss+1)->minorKey ^= Zobrist::piece(board.at(move.to()), move.to());
		}
		// Add to sq pawn
		if (move.typeOf() != Move::PROMOTION)
			(ss+1)->pawnKey ^= Zobrist::piece(board.at(move.from()), move.to());
		else {
			(ss+1)->nonPawnKey[stm] ^= Zobrist::piece(Piece(move.promotionType(), stm), move.to());
			if (isMajor(move.promotionType()))
				(ss+1)->majorKey ^= Zobrist::piece(Piece(move.promotionType(), stm), move.to());
			if (isMinor(move.promotionType()))
				(ss+1)->minorKey ^= Zobrist::piece(Piece(move.promotionType(), stm), move.to());
		}
	}
	else {
		
		if (move.typeOf() != Move::CASTLING){
			// Remove from sq piece
			(ss+1)->nonPawnKey[stm] ^= Zobrist::piece(board.at(move.from()), move.from());
			// Add the moving piece at the to sq
			(ss+1)->nonPawnKey[stm] ^= Zobrist::piece(board.at(move.from()), move.to());

			if (isMajor(from)) {
				// Remove from, add to
				(ss+1)->majorKey ^= Zobrist::piece(board.at(move.from()), move.from());
				(ss+1)->majorKey ^= Zobrist::piece(board.at(move.from()), move.to());
			}
			if (isMinor(from)) {
				// Remove from, add to
				(ss+1)->minorKey ^= Zobrist::piece(board.at(move.from()), move.from());
				(ss+1)->minorKey ^= Zobrist::piece(board.at(move.from()), move.to());
			}
			// If capture remove to
			if (to == PieceType::PAWN) {
				(ss+1)->pawnKey ^= Zobrist::piece(board.at(move.to()), move.to());
			}
			else if (to != PieceType::NONE) {
				(ss+1)->nonPawnKey[~stm] ^= Zobrist::piece(board.at(move.to()), move.to());
				if (isMajor(to)) 
					(ss+1)->majorKey ^= Zobrist::piece(board.at(move.to()), move.to());
				if (isMinor(to)) 
					(ss+1)->minorKey ^= Zobrist::piece(board.at(move.to()), move.to());
			}
			
		}
	}

	board.makeMove(move);
	

	if (move.typeOf() == Move::ENPASSANT || move.typeOf() == Move::PROMOTION){
		// For now just recalculate on special moves like these
		acc.refresh(board);
	}
	else if (move.typeOf() == Move::CASTLING){

		Square king = move.from();
		Square kingTo = (king > move.to()) ? king - 2 : king + 2;
		Square rookTo = (king > move.to()) ? kingTo + 1 : kingTo - 1;

		// Remove the king from sq
		(ss+1)->minorKey ^= Zobrist::piece(Piece(PieceType::KING, stm), king);
		(ss+1)->majorKey ^= Zobrist::piece(Piece(PieceType::KING, stm), king);
		(ss+1)->nonPawnKey[stm] ^= Zobrist::piece(Piece(PieceType::KING, stm), king);
		// Add him to the to sq
		(ss+1)->minorKey ^= Zobrist::piece(Piece(PieceType::KING, stm), kingTo);
		(ss+1)->majorKey ^= Zobrist::piece(Piece(PieceType::KING, stm), kingTo);
		(ss+1)->nonPawnKey[stm] ^= Zobrist::piece(Piece(PieceType::KING, stm), kingTo);

		// Remove the rook
		(ss+1)->majorKey ^= Zobrist::piece(Piece(PieceType::ROOK, stm), move.to());
		(ss+1)->nonPawnKey[stm] ^= Zobrist::piece(Piece(PieceType::ROOK, stm), move.to());
		// Add the rook
		(ss+1)->majorKey ^= Zobrist::piece(Piece(PieceType::ROOK, stm), rookTo);
		(ss+1)->nonPawnKey[stm] ^= Zobrist::piece(Piece(PieceType::ROOK, stm), rookTo);

		// There are basically just 2 quiet moves now for the accumulator
		// Move king and move rook
		// Since moves are encoded as king takes rook, its very easy
		acc.quiet(stm, kingTo, PieceType::KING, move.from(), PieceType::KING);
		acc.quiet(stm, rookTo, PieceType::ROOK, move.to(), PieceType::ROOK);
	}
	else if (to != PieceType::NONE){
		acc.capture(stm, move.to(), from, move.from(), from, move.to(), to);
	}
	else
		acc.quiet(stm, move.to(), from, move.from(), from);

}


void UnmakeMove(Board &board, Accumulator &acc, Move move){
	if (move == Move::NULL_MOVE){
		board.unmakeNullMove();
		return;
	}
	board.unmakeMove(move);

	PieceType to = board.at<PieceType>(move.to());
	PieceType from = board.at<PieceType>(move.from());
	Color stm = board.sideToMove();

	if (move.typeOf() == Move::ENPASSANT || move.typeOf() == Move::PROMOTION){
		// For now just recalculate on special moves like these
		acc.refresh(board);
	}
	else if (move.typeOf() == Move::CASTLING){
		Square king = move.from();
		Square kingTo = (king > move.to()) ? king - 2 : king + 2;
		Square rookTo = (king > move.to()) ? kingTo + 1 : kingTo - 1;
		// There are basically just 2 quiet moves now
		acc.quiet(stm, move.from(), PieceType::KING, kingTo, PieceType::KING);
		acc.quiet(stm, move.to(), PieceType::ROOK, rookTo, PieceType::ROOK);
	}
	else if (to != PieceType::NONE){
		acc.uncapture(stm, move.from(), from, move.to(), to, move.to(), from);
	}
	else {
		acc.quiet(stm, move.from(), from, move.to(), from);
	}
}

namespace Search {
	std::array<std::array<std::array<int, 219>, MAX_PLY + 1>, 2> lmrTable;

	bool isWin(int score){
		return score >= FOUND_MATE;
	}

	bool isLoss(int score){
		return score <= GETTING_MATED;
	}
	bool isMateScore(int score){
		return std::abs(score) >= FOUND_MATE;
	}
	void fillLmr(){
		// https://www.chessprogramming.org/Late_Move_Reductions
		// Neural Network based log formula
	}
	int scoreMove(Move move, Move ttMove, Stack *ss, ThreadInfo &thread){
		// MVV-LVA
		// TT Move
		// Killer Move Heuristic
		// Butterfly History
		// Continuation History
		// Capture History
		// SEE Ordering

		PieceType to = thread.board.at<PieceType>(move.to());
		if (move == ttMove){
			return 1000000;
		}
		else if (to != PieceType::NONE) {
			// MVV-Capthist-SEE
			int hist = thread.getCapthist(thread.board, move);
			int score = hist + MVV_VALUES[to];
			return 500000 + score - 800000 * !SEE(thread.board, move, -PawnValue);
		}
		else if (move == ss->killer){
			return 400000;
		}
		else {
			// Quiet non killers
			// main history + continuation history
			return thread.getQuietHistory(thread.board, move, ss);
		}
		return -1000000;
	}
	bool scoreComparator(Move &a, Move &b){
		return a.score() > b.score();
	}
	void pickMove(Movelist &mvlst, int start){
		for (int i=start+1;i<mvlst.size();i++){
			if (mvlst[i].score() > mvlst[start].score()){
				std::iter_swap(mvlst.begin() + start, mvlst.begin() + i);
			}
		}
	}	
	template<bool isPV>
	int qsearch(int ply, int alpha, const int beta, Stack *ss, ThreadInfo &thread, Limit &limit){
		if (thread.type == ThreadType::MAIN && (limit.outOfTime() || limit.outOfNodes(thread.nodes))){
			thread.searcher->stopSearching();
		}
		if (thread.stopped || thread.exiting || ply >= MAX_PLY - 1){
			return (ply >= MAX_PLY - 1 && !thread.board.inCheck()) ? network.inference(&thread.board, thread.accumulator) : 0;
		}

		ss->ply = ply;

		TTEntry *ttEntry = thread.TT.getEntry(thread.board.hash());
		bool ttHit = ttEntry->zobrist == static_cast<uint32_t>(thread.board.hash());
		if (!isPV && ttHit
			&& (ttEntry->flag == TTFlag::EXACT 
				|| (ttEntry->flag == TTFlag::BETA_CUT && ttEntry->score >= beta)
				|| (ttEntry->flag == TTFlag::FAIL_LOW && ttEntry->score <= alpha))){
			return ttEntry->score;
		}

		bool inCheck = thread.board.inCheck();
		int rawStaticEval, eval;

		// Get the corrected static eval if not in check
		if (inCheck){
			rawStaticEval = -INFINITE;
			eval = -INFINITE + ply;
		}
		else {
			rawStaticEval = ttHit ? ttEntry->staticEval : network.inference(&thread.board, thread.accumulator);
			eval = thread.correctStaticEval(ss, thread.board, rawStaticEval);
			// TT Static Eval
			if (ttHit && (
				ttEntry->flag == TTFlag::EXACT || 
				(ttEntry->flag == TTFlag::BETA_CUT && ttEntry->score >= eval) ||
				(ttEntry->flag == TTFlag::FAIL_LOW && ttEntry->score <= eval)
			)) 
				eval = ttEntry->score;

			if (eval >= beta)
				return eval;
			if (eval > alpha)
				alpha = eval;
		}

		int bestScore = eval;
		int moveCount = 0;
		Move qBestMove = Move::NO_MOVE;
		uint8_t ttFlag = TTFlag::FAIL_LOW;

		Movelist moves;

		// If we're in check, check all evasions
		if (!inCheck)
			movegen::legalmoves<movegen::MoveGenType::CAPTURE>(moves, thread.board);
		else
			movegen::legalmoves(moves, thread.board);

		// Move Scoring
		for (auto &move : moves){
			// Qsearch doesnt have killers
			// Still pass to make compiler happy
			move.setScore(scoreMove(move, Move(ttEntry->move), ss, thread));
		}
		for (int m_ = 0;m_<moves.size();m_++){
			if (thread.stopped || thread.exiting)
				return bestScore;
			// Move ordering
			pickMove(moves, m_);
			Move move = moves[m_];

			// SEE Pruning
			if (bestScore > GETTING_MATED && !SEE(thread.board, move, 0))
				continue;


			MakeMove(thread.board, thread.accumulator, move, ss);
			thread.nodes++;
			moveCount++;
			int score = -qsearch<isPV>(ply+1, -beta, -alpha, ss+1, thread, limit);
			UnmakeMove(thread.board, thread.accumulator, move);

			if (score > bestScore){
				bestScore = score;
				if (score > alpha){
					alpha = score;
					qBestMove = move;
					ttFlag = TTFlag::EXACT;
				}
			}
			if (score >= beta){
				ttFlag = TTFlag::BETA_CUT;
				break;
			}
		}
		if (!moveCount && inCheck)
			return -MATE + ply;

		ttEntry->updateEntry(thread.board.hash(), qBestMove, bestScore, std::clamp(rawStaticEval, -INFINITE, INFINITE), ttFlag, 0);

		return bestScore;

	}
	template<bool isPV>
	int search(int depth, int ply, int alpha, int beta, Stack *ss, ThreadInfo &thread, Limit &limit){
		//bool isPV = alpha != beta - 1;
		bool root = ply == 0;
		ss->ply = ply;

		if (isPV) 
			ss->pv.length = 0;
		if (depth <= 0){
			return qsearch<isPV>(ply, alpha, beta, ss, thread, limit);
		}

		// Terminal Conditions (and checkmate)
		if (!root){
			if (thread.board.isRepetition(1) || thread.board.isHalfMoveDraw())
				return 0;

			if (thread.type == ThreadType::MAIN && (limit.outOfTime() || limit.outOfNodes(thread.nodes)) && thread.rootDepth != 1 ){
				thread.searcher->stopSearching();
			}
			if (thread.stopped || thread.exiting || ply >= MAX_PLY - 1){
				return (ply >= MAX_PLY - 1 && !thread.board.inCheck()) ? network.inference(&thread.board, thread.accumulator) : 0;
			}
		}



		TTEntry *ttEntry = thread.TT.getEntry(thread.board.hash());
		bool ttHit = moveIsNull(ss->excluded) && ttEntry->zobrist == static_cast<uint32_t>(thread.board.hash());
		if (!isPV && ttHit && ttEntry->depth >= depth
			&& (ttEntry->flag == TTFlag::EXACT 
				|| (ttEntry->flag == TTFlag::BETA_CUT && ttEntry->score >= beta)
				|| (ttEntry->flag == TTFlag::FAIL_LOW && ttEntry->score <= alpha))){
			return ttEntry->score;
		}
		bool notHashMove = !ttHit || moveIsNull(Move(ttEntry->move));

		// http://talkchess.com/forum3/viewtopic.php?f=7&t=74769&sid=64085e3396554f0fba414404445b3120
    	// https://github.com/jhonnold/berserk/blob/dd1678c278412898561d40a31a7bd08d49565636/src/search.c#L379
    	// https://github.com/PGG106/Alexandria/blob/debbf941889b28400f9a2d4b34515691c64dfae6/src/search.cpp#L636
		bool canIIR = notHashMove && depth >= IIR_MIN_DEPTH();

		int bestScore = -INFINITE;
		int oldAlpha = alpha;
		int rawStaticEval = GETTING_MATED;
		int score = bestScore;
		int moveCount = 0;
		bool inCheck = thread.board.inCheck();
		ss->conthist = nullptr;
		// Get the corrected static evaluation if we're not in singular search or check
		if (moveIsNull(ss->excluded)){
			if (inCheck){
				ss->staticEval = rawStaticEval = -INFINITE;
				ss->eval = -INFINITE;
			}
			else {
				rawStaticEval = ttHit ? ttEntry->staticEval : network.inference(&thread.board, thread.accumulator);
				ss->staticEval = thread.correctStaticEval(ss, thread.board, rawStaticEval);
				ss->eval = ss->staticEval;
				// TT Static Eval
				if (ttHit && (
					ttEntry->flag == TTFlag::EXACT || 
					(ttEntry->flag == TTFlag::BETA_CUT && ttEntry->score >= ss->staticEval) ||
					(ttEntry->flag == TTFlag::FAIL_LOW && ttEntry->score <= ss->staticEval)
				)) 
					ss->eval = ttEntry->score;
			}
		}

		

		// Improving heurstic
		// We are better than 2 plies ago
		bool improving = !inCheck && ply > 1 && (ss - 2)->staticEval < ss->staticEval;
		uint8_t ttFlag = TTFlag::FAIL_LOW;
		// Pruning
		
		if (!root && !isPV && !inCheck && moveIsNull(ss->excluded)){
			// Reverse Futility Pruning
			if (depth <= RFP_MAX_DEPTH() && ss->eval - RFP_MARGIN() * (depth - improving) >= beta)
				return ss->eval;

			// Null Move Pruning
			Bitboard nonPawns = thread.board.us(thread.board.sideToMove()) ^ thread.board.pieces(PieceType::PAWN, thread.board.sideToMove());
			if (depth >= 2 && ss->eval >= beta && ply > thread.minNmpPly && !nonPawns.empty()){
				// Sirius formula
				const int reduction = NMP_BASE_REDUCTION() + depth / NMP_REDUCTION_SCALE() + std::min(2, (ss->eval-beta)/NMP_EVAL_SCALE());

				ss->conthist = nullptr;

				MakeMove(thread.board, thread.accumulator, Move(Move::NULL_MOVE), ss);
				int nmpScore = -search<false>(depth-reduction, ply+1, -beta, -beta + 1, ss+1, thread, limit);
				UnmakeMove(thread.board, thread.accumulator, Move(Move::NULL_MOVE));

				if (nmpScore >= beta){
					// Zugzwang verifiction
					// All "real" moves are bad, so doing a null causes a cutoff
					// do a reduced search to verify and if that also fails high
					// then all is well, else dont prune
					if (depth <= 15 || thread.minNmpPly > 0)
						return isWin(nmpScore) ? beta : nmpScore;
					thread.minNmpPly = ply + (depth - reduction) * 3 / 4;
					int verification = search<false>(depth-NMP_BASE_REDUCTION(), ply+1, beta-1, beta, ss, thread, limit);
					thread.minNmpPly = 0;
					if (verification >= beta)
						return verification;
				}
			}
			// Internal Iterative Reduction
			if (canIIR)
				depth -= 1;
		}
		
		// Thought
		// What if we arrange a vector C = {....} of weights and input of say {alpha, beta, eval...}
		// and use some sort of data generation method to create a pruning heuristic
		// with something like sigmoid(C dot I) >= 0.75 ?


		Move bestMove = Move::NO_MOVE;
		Movelist moves;
		Movelist seenQuiets;
		Movelist seenCaptures;

		movegen::legalmoves(moves, thread.board);

		// Move Scoring
		for (auto &move : moves){
			move.setScore(scoreMove(move, Move(ttEntry->move), ss, thread));
		}
		if (root) {
			bestMove = moves[0]; // Guaruntee some random move
		}

		// Other vars
		bool skipQuiets = false;
		for (int m_ = 0;m_<moves.size();m_++){
			if (thread.stopped || thread.exiting)
				return bestScore;

			pickMove(moves, m_); // We do this so that the swapped moves stays at the front 
			Move move = moves[m_];
			bool isQuiet = !thread.board.isCapture(move);

			if (move == ss->excluded)
				continue;
			if (isQuiet && skipQuiets)
				continue;
			if (isQuiet)
				seenQuiets.add(move);
			else
				seenCaptures.add(move);

			ss->historyScore = isQuiet ? thread.getQuietHistory(thread.board, move, ss) : thread.getCapthist(thread.board, move);

			if (!root && bestScore > GETTING_MATED){
				// Late Move Pruning
			#ifndef STORE_LMR_DATA
				if (!isPV && !inCheck && moveCount >= LMP_MIN_MOVES_BASE() + depth * depth / (2 - improving))
					break;
			#endif

				if (!SEE(thread.board, move, SEE_PRUNING_SCALAR() * depth))
					continue;

				// History Pruning
				// Failed, will test again later
				// https://github.com/aronpetko/integral/blob/733036df88408d0d6338d05f7991f46f0527ed4f/src/engine/search/search.cc#L945
				// https://github.com/mcthouacbb/Sirius/blob/15501c19650f53f0a10973695a6d284bc243bf7d/Sirius/src/search.cpp#L614
				// if (isQuiet && depth <= HIST_PRUNING_DEPTH && ss->historyScore <= HIST_PRUNING_MARGIN * depth){
				// 	skipQuiets = true;
				// 	continue;
				// }
			}

			// Singular Extensions
			// Sirius conditions
			// https://github.com/mcthouacbb/Sirius/blob/15501c19650f53f0a10973695a6d284bc243bf7d/Sirius/src/search.cpp#L620
			bool doSE = !root && moveIsNull(ss->excluded) &&
						depth >= SE_MIN_DEPTH() && Move(ttEntry->move) == move && ttEntry->depth >= depth - 3
						&& ttEntry->flag != TTFlag::FAIL_LOW && !isMateScore(ttEntry->score);	
			
			int extension = 0;

			if (doSE) {
				int sBeta = std::max(-MATE, ttEntry->score - SE_BETA_SCALE() * depth / 16);
				int sDepth = (depth - 1) / 2;
				// How good are we without this move
				ss->excluded = Move(ttEntry->move);
				int seScore = search<false>(sDepth, ply+1, sBeta-1, sBeta, ss, thread, limit);
				ss->excluded = Move::NO_MOVE;

				if (seScore < sBeta) {
					if (!isPV && seScore < sBeta - SE_DOUBLE_MARGIN())
						extension = 2; // Double extension
					else
						extension = 1; // Singular Extension
				}
				else if (ttEntry->score >= beta)
					extension = -2 + isPV; // Negative Extension

			}					

			// Update Continuation History
			ss->conthist = thread.getConthistSegment(thread.board, move);

			MakeMove(thread.board, thread.accumulator, move, ss);
			moveCount++;
			thread.nodes++;
			

			int newDepth = depth - 1 + extension;
			// Late Move Reduction
			bool doLMR = depth >= LMR_MIN_DEPTH() && moveCount > LMR_MIN_MOVECOUNT() && !thread.board.inCheck();

		#ifdef STORE_LMR_DATA
			bool didLMR = false;
			LMRInfo lmrEntry;
			if (doLMR) {
				lmrEntry = LMRInfo(depth, moveCount, isQuiet, lmrIndex(std::min(depth, 31), std::min(moveCount, 31), isQuiet) + !isPV - improving - ss->historyScore / LMR_HIST_DIVISOR());
				didLMR = true;
			}
			doLMR = false;
		#endif

			if (doLMR){
				int reduction = lmrIndex(std::min(depth, 31), std::min(moveCount, 31), isQuiet);
				//int reduction = lmrForward(depth, moveCount, isQuiet) + 0.5;
				// Reduce more if not a PV node
				reduction += !isPV;
				// Reduce less when improving
				reduction -= improving;
				// Reduce less if good history
				reduction -= ss->historyScore / LMR_HIST_DIVISOR();

				score = -search<false>(newDepth - reduction, ply+1, -alpha - 1, -alpha, ss+1, thread, limit);
				// Re-search at normal depth
				if (score > alpha)
					score = -search<false>(newDepth, ply+1, -alpha - 1, -alpha, ss+1, thread, limit);
			}
			else if (!isPV || moveCount > 1){
				score = -search<false>(newDepth, ply+1, -alpha - 1, -alpha, ss+1, thread, limit);
			}
			if (isPV && (moveCount == 1 || score > alpha)){
				score = -search<isPV>(newDepth, ply+1, -beta, -alpha, ss+1, thread, limit);
			}

			UnmakeMove(thread.board, thread.accumulator, move);
			
			#ifdef STORE_LMR_DATA
				if (didLMR){
					if (score > alpha)
						lmrEntry.optimalReduction = 0;
					thread.lmrInfo.push_back(lmrEntry);
					//thread.lmrInfo[thread.lmrInfo.size()-1].optimalReduction = 0;
				}
			#endif
			if (score > bestScore){
				bestScore = score;
				if (score > alpha){
					bestMove = move;
					if (root) {
						thread.bestMove = bestMove;
						thread.threadBestScore = bestScore;
					}
					ttFlag = TTFlag::EXACT;
					alpha = score;
					if (isPV){
						ss->pv.update(move, (ss+1)->pv);
					}
				}
			}


			if (score >= beta){
				ttFlag = TTFlag::BETA_CUT;
				ss->killer = isQuiet ? bestMove : Move::NO_MOVE;
				// Butterfly History
				// Continuation History
				// Capture History
				int bonus = std::min(HIST_BONUS_QUADRATIC() * depth * depth + HIST_BONUS_LINEAR() * depth - HIST_BONUS_OFFSET(), 2048);
				int malus = std::min(-(HIST_MALUS_QUADRATIC() * depth * depth + HIST_MALUS_LINEAR() * depth + HIST_MALUS_OFFSET()), 1024);
				if (isQuiet){
					thread.updateHistory(thread.board.sideToMove(), move, bonus);
					thread.updateConthist(ss, thread.board, move, bonus);
					for (const Move quietMove : seenQuiets){
						if (quietMove == move)
							continue;
						thread.updateHistory(thread.board.sideToMove(), quietMove, malus);
						thread.updateConthist(ss, thread.board, quietMove, malus);
					}
				}
				else {
					thread.updateCapthist(thread.board, move, bonus);
				}
				// Always malus captures
				for (const Move noisyMove : seenCaptures){
					if (noisyMove == move)
						continue;
					thread.updateCapthist(thread.board, noisyMove, malus);
				}
				break;
			}
			
		}
		if (!moveCount){
			return inCheck ? -MATE + ply : 0;
		}
		if (moveIsNull(ss->excluded)){
			// Update correction history
			bool isBestQuiet = !thread.board.isCapture(bestMove);
			//ttFlag = bestScore >= beta ? TTFlag::BETA_CUT : alpha != oldAlpha ? TTFlag::EXACT : TTFlag::FAIL_LOW;
			if (!inCheck && (isBestQuiet || moveIsNull(bestMove))
				&& (ttFlag == TTFlag::EXACT 
					|| ttFlag == TTFlag::BETA_CUT && bestScore > ss->staticEval 
					|| ttFlag == TTFlag::FAIL_LOW && bestScore < ss->staticEval)) {
				int bonus = static_cast<int>((CORRHIST_BONUS_WEIGHT() / 100.0) * (bestScore - ss->staticEval) * depth / 8);
				thread.updateCorrhist(ss, thread.board, bonus);
			}

			// Update TT
			//TTEntry *tableEntry = thread.TT.getEntry(thread.board.hash());
			ttEntry->updateEntry(thread.board.hash(), bestMove, bestScore, std::clamp(rawStaticEval, -INFINITE, INFINITE), ttFlag, depth);
			//*ttEntry = TTEntry(thread.board.hash(), ttFlag == TTFlag::FAIL_LOW ? ttEntry->move : bestMove, bestScore, ttFlag, depth);
		}
		return bestScore;

	}

	int iterativeDeepening(Board &board, ThreadInfo &threadInfo, Limit limit, Searcher *searcher){
		threadInfo.board = board;
		threadInfo.accumulator.refresh(threadInfo.board);

		bool isMain = threadInfo.type == ThreadType::MAIN;

		auto stack = std::make_unique<std::array<Stack, MAX_PLY+3>>();
		Stack *ss = reinterpret_cast<Stack*>(stack->data()+2); // Saftey for conthist
		std::memset(stack.get(), 0, sizeof(Stack) * (MAX_PLY+3));

		PVList lastPV{};
		int score = -INFINITE;
		int lastScore = -INFINITE;

		int moveEval = -INFINITE;
		int smpDepth = isMain ? 0 : threadInfo.threadId;
		int64_t avgnps = 0;
		for (int depth = 1;depth<=limit.depth;depth++){
			auto aborted = [&]() {
				if (threadInfo.stopped)
					return true;
				if (isMain)
					return limit.outOfTime() || limit.outOfNodes(threadInfo.nodes) || limit.softNodes(threadInfo.nodes);
				else
					return limit.softNodes(threadInfo.nodes);
			};
			threadInfo.rootDepth = depth;
			ss->pawnKey = resetPawnHash(threadInfo.board);
			ss->majorKey = resetMajorHash(threadInfo.board);
			ss->minorKey = resetMinorHash(threadInfo.board);
			ss->nonPawnKey[0] = resetNonPawnHash(threadInfo.board, Color::WHITE);
			ss->nonPawnKey[1] = resetNonPawnHash(threadInfo.board, Color::BLACK);

			// Aspiration Windows
			if (depth >= MIN_ASP_WINDOW_DEPTH()){
				int delta = INITIAL_ASP_WINDOW();
				int alpha  = std::max(lastScore - delta, -INFINITE);
				int beta = std::min(lastScore + delta, INFINITE);
				int aspDepth = depth;
				while (!aborted()){
					score = search<true>(std::max(aspDepth, 1), 0, alpha, beta, ss, threadInfo, limit);
					if (score <= alpha){
						beta = (alpha + beta) / 2;
						alpha = std::max(alpha - delta, -INFINITE);
						aspDepth = depth;
					}
					else {
						if (score >= beta){
							beta = std::min(beta + delta, INFINITE);
							aspDepth = std::max(aspDepth-1, depth-5);
						}
						else
							break;
					}
					delta += delta * ASP_WIDENING_FACTOR() / 16;
				}
			}
			else
				score = search<true>(depth, 0, -INFINITE, INFINITE, ss, threadInfo, limit);
			// ---------------------
			if (depth != 1 && aborted()){
				break;
			}

			lastScore = score;
			lastPV = ss->pv;

			// Save best scores
			threadInfo.completed = depth;

			if (!isMain){
				continue;
			}

			// Reporting
			uint64_t nodecnt = (*searcher).nodeCount();
			
			std::stringstream pvss; // String stream for the mainline
			Board pvBoard = threadInfo.board; // Test board for WDL and eval normalization since we need the final board state of the mainline
			for (int i=0;i<lastPV.length;i++){
				pvss << uci::moveToUci(lastPV.moves[i]) << " ";
				pvBoard.makeMove(lastPV.moves[i]);
			}
			if (searcher->printInfo) {
				std::cout << "info depth " << depth << " score ";
				if (score >= FOUND_MATE || score <= GETTING_MATED){
					std::cout << "mate " << ((score < 0) ? "-" : "") << (MATE - std::abs(score)) / 2 + 1;
				}
				else {
					std::cout << "cp " << scaleEval(score, pvBoard);
					if (searcher->showWDL){
						WDL wdl = computeWDL(score, pvBoard);
						std::cout << " wdl " << wdl.w << " " << wdl.d << " " << wdl.l;
					}
				}
				std::cout << " hashfull " << searcher->TT.hashfull();
				std::cout << " nodes " << nodecnt << " nps " << nodecnt / (limit.timer.elapsed()+1) * 1000 << " pv ";
				std::cout << pvss.str() << std::endl;
			}	
			if (limit.outOfTimeSoft())
				break;

		}
		// if (isMain){
		// 	searcher->stopSearching();
		// 	searcher->waitForWorkersFinished();
		// 	std::cout << "bestmove " << uci::moveToUci(lastPV.moves[0]) << std::endl;
		// }

		threadInfo.bestMove = lastPV.moves[0];
		return lastScore;
	}

}