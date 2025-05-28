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
void MakeMove(Board &board, Accumulator &acc, Move &move, Search::Stack *ss){
	PieceType to = board.at<PieceType>(move.to());
	PieceType from = board.at<PieceType>(move.from());
	Color stm = board.sideToMove();

	// Pawn key incremental updates
	ss->pawnKey = (ss-1)->pawnKey;
	if (from == PieceType::PAWN){
		// Update pawn zobrist key
		// Remove from sq pawn
		ss->pawnKey ^= Zobrist::piece(board.at(move.from()), move.from());
		if (move.typeOf() == Move::ENPASSANT){
			// Remove en passant pawn
			ss->pawnKey ^= Zobrist::piece(Piece(PieceType::PAWN, ~stm), move.to().ep_square());
		}
		else if (to == PieceType::PAWN){
			// Remove to sq pawn
			ss->pawnKey ^= Zobrist::piece(board.at(move.to()), move.to());
		}
		// Add to sq pawn
		if (move.typeOf() != Move::PROMOTION)
			ss->pawnKey ^= Zobrist::piece(board.at(move.from()), move.to());
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
		// There are basically just 2 quiet moves now
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

void UnmakeMove(Board &board, Accumulator &acc, Move &move){
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
		// Weiss formula for reductions is
		// Captures/Promo: 0.2 + log(depth) * log(movecount) / 3.35
		// Quiets: 		   1.35 + log(depth) * log(movecount) / 2.75
		// https://www.chessprogramming.org/Late_Move_Reductions
		for (int isQuiet = 0;isQuiet<=1;isQuiet++){
			for (size_t depth=0;depth <= MAX_PLY;depth++){
				for (int movecount=0;movecount<=218;movecount++){
					if (depth == 0 || movecount == 0){
						lmrTable[isQuiet][depth][movecount] = 0;
						continue;
					}
					if (isQuiet){
						lmrTable[isQuiet][depth][movecount] = 1.35 + std::log(depth) * std::log(movecount) / 2.75;
					}
					else {
						lmrTable[isQuiet][depth][movecount] = 0.2 + std::log(depth) * std::log(movecount) / 3.35;
					}
				}
			}
		}
	}
	int scoreMove(Move &move, Move &ttMove, Stack *ss, ThreadInfo &thread){
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
		TTEntry *ttEntry = thread.TT.getEntry(thread.board.hash());
		bool ttHit = ttEntry->zobrist == thread.board.hash();
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
			rawStaticEval = network.inference(&thread.board, thread.accumulator);
			eval = thread.correctStaticEval(ss, thread.board, rawStaticEval);
			if (eval >= beta)
				return eval;
			if (eval > alpha)
				alpha = eval;
		}

		int bestScore = eval;
		int moveCount = 0;

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
			move.setScore(scoreMove(move, ttEntry->move, ss, thread));
		}
		for (int m_ = 0;m_<moves.size();m_++){
			if (thread.abort.load(std::memory_order_relaxed))
				return bestScore;
			if (limit.outOfTime() || limit.outOfNodes(thread.nodes)){
				thread.abort.store(true, std::memory_order_relaxed);
				return bestScore;
			}
			
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
				}
			}
			if (score >= beta){
				break;
			}
		}
		if (!moveCount && inCheck)
			return -MATE + ply;
		return bestScore;

	}
	template<bool isPV>
	int search(int depth, int ply, int alpha, int beta, Stack *ss, ThreadInfo &thread, Limit &limit){
		//bool isPV = alpha != beta - 1;
		bool root = ply == 0;
		if (isPV) 
			ss->pv.length = 0;
		if (depth <= 0){
			return qsearch<isPV>(ply, alpha, beta, ss, thread, limit);
		}

		// Terminal Conditions (and checkmate)
		if (!root){
			if (thread.board.isRepetition(1) || thread.board.isHalfMoveDraw())
				return 0;
		}


		TTEntry *ttEntry = thread.TT.getEntry(thread.board.hash());
		bool ttHit = moveIsNull(ss->excluded) && ttEntry->zobrist == thread.board.hash();
		if (!isPV && ttHit && ttEntry->depth >= depth
			&& (ttEntry->flag == TTFlag::EXACT 
				|| (ttEntry->flag == TTFlag::BETA_CUT && ttEntry->score >= beta)
				|| (ttEntry->flag == TTFlag::FAIL_LOW && ttEntry->score <= alpha))){
			return ttEntry->score;
		}
		bool hashMove = !ttHit || moveIsNull(ttEntry->move);

		// http://talkchess.com/forum3/viewtopic.php?f=7&t=74769&sid=64085e3396554f0fba414404445b3120
    	// https://github.com/jhonnold/berserk/blob/dd1678c278412898561d40a31a7bd08d49565636/src/search.c#L379
    	// https://github.com/PGG106/Alexandria/blob/debbf941889b28400f9a2d4b34515691c64dfae6/src/search.cpp#L636
		bool canIIR = hashMove && depth >= IIR_MIN_DEPTH;

		int bestScore = -INFINITE;
		int rawStaticEval = GETTING_MATED;
		int score = bestScore;
		int moveCount = 0;
		bool inCheck = thread.board.inCheck();

		// Get the corrected static evaluation if we're not in singular search or check
		if (moveIsNull(ss->excluded)){
			if (inCheck)
				ss->staticEval = rawStaticEval = -INFINITE;
			else {
				rawStaticEval = network.inference(&thread.board, thread.accumulator);
				ss->staticEval = thread.correctStaticEval(ss, thread.board, rawStaticEval);
			}
		}

		ss->conthist = nullptr;

		// Improving heurstic
		// We are better than 2 plies ago
		bool improving = !inCheck && ply > 1 && (ss - 2)->staticEval < ss->staticEval;
		uint8_t ttFlag = TTFlag::FAIL_LOW;
		// Pruning
		
		if (!root && !isPV && !inCheck && moveIsNull(ss->excluded)){
			// Reverse Futility Pruning
			if (ss->staticEval - RFP_MARGIN * (depth - improving) >= beta && depth <= RFP_MAX_DEPTH)
				return ss->staticEval;

			// Null Move Pruning
			Bitboard nonPawns = thread.board.us(thread.board.sideToMove()) ^ thread.board.pieces(PieceType::PAWN, thread.board.sideToMove());
			if (depth >= 2 && ss->staticEval >= beta && ply > thread.minNmpPly && !nonPawns.empty()){
				// Sirius formula
				const int reduction = NMP_BASE_REDUCTION + depth / NMP_REDUCTION_SCALE + std::min(2, (ss->staticEval-beta)/NMP_EVAL_SCALE);
				thread.board.makeNullMove();
				int nmpScore = -search<false>(depth-reduction, ply+1, -beta, -beta + 1, ss+1, thread, limit);
				thread.board.unmakeNullMove();
				if (nmpScore >= beta){
					// Zugzwang verifiction
					// All "real" moves are bad, so doing a null causes a cutoff
					// do a reduced search to verify and if that also fails high
					// then all is well, else dont prune
					if (depth <= 15 || thread.minNmpPly > 0)
						return isWin(nmpScore) ? beta : nmpScore;
					thread.minNmpPly = ply + (depth - reduction) * 3 / 4;
					int verification = search<false>(depth-NMP_BASE_REDUCTION, ply+1, beta-1, beta, ss, thread, limit);
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
			move.setScore(scoreMove(move, ttEntry->move, ss, thread));
		}
		if (root)
			bestMove = moves[0]; // Guaruntee some random move
		// Other vars
		bool skipQuiets = false;
		for (int m_ = 0;m_<moves.size();m_++){
			if (thread.abort.load(std::memory_order_relaxed))
				return bestScore;
			if ( (limit.outOfTime() || limit.outOfNodes(thread.nodes)) && thread.rootDepth != 1 ){
				thread.abort.store(true, std::memory_order_relaxed);
				return bestScore;
			}
			pickMove(moves, m_);
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
				if (!isPV && !inCheck && moveCount >= LMP_MIN_MOVES_BASE + depth * depth / (2 - improving))
					break;

				// History Pruning
				// Failed, will test again later
				// https://github.com/aronpetko/integral/blob/733036df88408d0d6338d05f7991f46f0527ed4f/src/engine/search/search.cc#L945
				// https://github.com/mcthouacbb/Sirius/blob/15501c19650f53f0a10973695a6d284bc243bf7d/Sirius/src/search.cpp#L614
				// if (isQuiet && depth <= HIST_PRUNING_DEPTH && ss->historyScore <= HIST_PRUNING_MARGIN * depth){
				// 	skipQuiets = true;
				// 	continue;
				// }
			}


			ss->conthist = thread.getConthistSegment(thread.board, move);

			// Singular Extensions
			// Sirius conditions
			// https://github.com/mcthouacbb/Sirius/blob/15501c19650f53f0a10973695a6d284bc243bf7d/Sirius/src/search.cpp#L620
			bool doSE = !root && moveIsNull(ss->excluded) &&
						depth >= SE_MIN_DEPTH && ttEntry->move == move && ttEntry->depth >= depth - 3
						&& ttEntry->flag != TTFlag::FAIL_LOW && !isMateScore(ttEntry->score);	
			
			int extension = 0;

			if (doSE) {
				int sBeta = std::max(-MATE, ttEntry->score - SE_BETA_SCALE * depth / 16);
				int sDepth = (depth - 1) / 2;
				// How good are we without this move
				ss->excluded = ttEntry->move;
				int seScore = search<false>(sDepth, ply+1, sBeta-1, sBeta, ss, thread, limit);
				ss->excluded = Move::NO_MOVE;

				if (seScore < sBeta) {
					if (!isPV && seScore < sBeta - SE_DOUBLE_MARGIN)
						extension = 2; // Double extension
					else
						extension = 1; // Singular Extension
				}
				else if (ttEntry->score >= beta)
					extension = -2 + isPV; // Negative Extension

			}					

			MakeMove(thread.board, thread.accumulator, move, ss);
			moveCount++;
			thread.nodes++;
			

			int newDepth = depth - 1 + extension;
			// Late Move Reduction
			if (depth >= LMR_MIN_DEPTH && moveCount > 5 && !thread.board.inCheck()){
				int reduction = lmrTable[isQuiet && move.typeOf() != Move::PROMOTION][depth][moveCount];

				// Reduce more if not a PV node
				reduction += !isPV;

				score = -search<false>(newDepth-reduction, ply+1, -alpha - 1, -alpha, ss+1, thread, limit);
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
			if (score > bestScore){
				bestScore = score;
				if (score > alpha){
					bestMove = move;
					if (root)
						thread.bestMove = bestMove;
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
				int bonus = std::min(8 * depth * depth + 212 * depth - 150, 2048);
				int malus = std::min(-(5 * depth * depth + 250 * depth + 66), 1024);
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
			if (!inCheck && (isBestQuiet || moveIsNull(bestMove))
				&& (ttFlag == TTFlag::EXACT 
					|| ttFlag == TTFlag::BETA_CUT && bestScore > ss->staticEval 
					|| ttFlag == TTFlag::FAIL_LOW && bestScore < ss->staticEval)) {
				int bonus = (bestScore - ss->staticEval) * depth / 8;
				thread.updateCorrhist(ss, thread.board, bonus);
			}

			// Update TT
			*ttEntry = TTEntry(thread.board.hash(), ttFlag == TTFlag::FAIL_LOW ? ttEntry->move : bestMove, bestScore, ttFlag, depth);
		}
		return bestScore;

	}

	int iterativeDeepening(Board &board, ThreadInfo &threadInfo, Limit limit, Searcher *searcher){
		threadInfo.abort.store(false);
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
		int oldnodecnt = 0;
		double branchsum = 0;
		double avgbranchfac = 0;
		int64_t avgnps = 0;
		for (int depth=1;depth<=limit.depth;depth++){
			auto aborted = [&]() {
				if (isMain)
					return limit.outOfTime() || limit.outOfNodes(threadInfo.nodes) || limit.softNodes(threadInfo.nodes) || threadInfo.abort.load(std::memory_order_relaxed);
				else
					return limit.softNodes(threadInfo.nodes) || threadInfo.abort.load(std::memory_order_relaxed);
			};
			threadInfo.rootDepth = depth;
			(ss-1)->pawnKey = resetPawnHash(threadInfo.board);
			// Aspiration Windows (WIP untuned)
			if (depth >= MIN_ASP_WINDOW_DEPTH){
				int delta = INITIAL_ASP_WINDOW;
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
					delta += delta * ASP_WIDENING_FACTOR / 16;
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

			// Maybe useful info for diagnostics
			if (oldnodecnt != 0){
				branchsum += (double)threadInfo.nodes / oldnodecnt;
				avgbranchfac = branchsum / (depth-1);
			}
			avgnps = threadInfo.nodes / (limit.timer.elapsed()+1);
			oldnodecnt = threadInfo.nodes;
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

			std::cout << "info depth " << depth << " score ";
			if (score >= FOUND_MATE || score <= GETTING_MATED){
				std::cout << "mate " << ((score < 0) ? "-" : "") << (MATE - std::abs(score)) / 2 + 1;
			}
			else {
				std::cout << "cp " << score;
				if (searcher->showWDL){
					WDL wdl = computeWDL(score, pvBoard);
					std::cout << " wdl " << wdl.w << " " << wdl.d << " " << wdl.l;
				}
			}

			std::cout << " nodes " << nodecnt << " nps " << nodecnt / (limit.timer.elapsed()+1) * 1000 << " pv ";
			std::cout << pvss.str() << std::endl;

			if (limit.outOfTimeSoft())
				break;

		}
		
		if (isMain){
			std::cout << "bestmove " << uci::moveToUci(lastPV.moves[0]) << std::endl;
		}
		threadInfo.abort.store(true, std::memory_order_relaxed);

		threadInfo.bestMove = lastPV.moves[0];
		return lastScore;
	}

	// Benchmark for OpenBench
	void bench(){
	    int64_t totalNodes = 0;
	    int64_t totalMS = 0;

	    std::cout << "Benchmark started at depth " << (int)BENCH_DEPTH << std::endl;
	    // Thanks Prelude
	    std::array<std::string, 50> fens = {"r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq a6 0 14",
	                              "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 2 24",
	                              "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 16 42",
	                              "6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2 b - - 4 44",
	                              "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54",
	                              "7r/2p3k1/1p1p1qp1/1P1Bp3/p1P2r1P/P7/4R3/Q4RK1 w - - 0 36",
	                              "r1bq1rk1/pp2b1pp/n1pp1n2/3P1p2/2P1p3/2N1P2N/PP2BPPP/R1BQ1RK1 b - - 2 10",
	                              "3r3k/2r4p/1p1b3q/p4P2/P2Pp3/1B2P3/3BQ1RP/6K1 w - - 3 87",
	                              "2r4r/1p4k1/1Pnp4/3Qb1pq/8/4BpPp/5P2/2RR1BK1 w - - 0 42",
	                              "4q1bk/6b1/7p/p1p4p/PNPpP2P/KN4P1/3Q4/4R3 b - - 0 37",
	                              "2q3r1/1r2pk2/pp3pp1/2pP3p/P1Pb1BbP/1P4Q1/R3NPP1/4R1K1 w - - 2 34",
	                              "1r2r2k/1b4q1/pp5p/2pPp1p1/P3Pn2/1P1B1Q1P/2R3P1/4BR1K b - - 1 37",
	                              "r3kbbr/pp1n1p1P/3ppnp1/q5N1/1P1pP3/P1N1B3/2P1QP2/R3KB1R b KQkq b3 0 17",
	                              "8/6pk/2b1Rp2/3r4/1R1B2PP/P5K1/8/2r5 b - - 16 42",
	                              "1r4k1/4ppb1/2n1b1qp/pB4p1/1n1BP1P1/7P/2PNQPK1/3RN3 w - - 8 29",
	                              "8/p2B4/PkP5/4p1pK/4Pb1p/5P2/8/8 w - - 29 68",
	                              "3r4/ppq1ppkp/4bnp1/2pN4/2P1P3/1P4P1/PQ3PBP/R4K2 b - - 2 20",
	                              "5rr1/4n2k/4q2P/P1P2n2/3B1p2/4pP2/2N1P3/1RR1K2Q w - - 1 49",
	                              "1r5k/2pq2p1/3p3p/p1pP4/4QP2/PP1R3P/6PK/8 w - - 1 51",
	                              "q5k1/5ppp/1r3bn1/1B6/P1N2P2/BQ2P1P1/5K1P/8 b - - 2 34",
	                              "r1b2k1r/5n2/p4q2/1ppn1Pp1/3pp1p1/NP2P3/P1PPBK2/1RQN2R1 w - - 0 22",
	                              "r1bqk2r/pppp1ppp/5n2/4b3/4P3/P1N5/1PP2PPP/R1BQKB1R w KQkq - 0 5",
	                              "r1bqr1k1/pp1p1ppp/2p5/8/3N1Q2/P2BB3/1PP2PPP/R3K2n b Q - 1 12",
	                              "r1bq2k1/p4r1p/1pp2pp1/3p4/1P1B3Q/P2B1N2/2P3PP/4R1K1 b - - 2 19",
	                              "r4qk1/6r1/1p4p1/2ppBbN1/1p5Q/P7/2P3PP/5RK1 w - - 2 25",
	                              "r7/6k1/1p6/2pp1p2/7Q/8/p1P2K1P/8 w - - 0 32",
	                              "r3k2r/ppp1pp1p/2nqb1pn/3p4/4P3/2PP4/PP1NBPPP/R2QK1NR w KQkq - 1 5",
	                              "3r1rk1/1pp1pn1p/p1n1q1p1/3p4/Q3P3/2P5/PP1NBPPP/4RRK1 w - - 0 12",
	                              "5rk1/1pp1pn1p/p3Brp1/8/1n6/5N2/PP3PPP/2R2RK1 w - - 2 20",
	                              "8/1p2pk1p/p1p1r1p1/3n4/8/5R2/PP3PPP/4R1K1 b - - 3 27",
	                              "8/4pk2/1p1r2p1/p1p4p/Pn5P/3R4/1P3PP1/4RK2 w - - 1 33",
	                              "8/5k2/1pnrp1p1/p1p4p/P6P/4R1PK/1P3P2/4R3 b - - 1 38",
	                              "8/8/1p1kp1p1/p1pr1n1p/P6P/1R4P1/1P3PK1/1R6 b - - 15 45",
	                              "8/8/1p1k2p1/p1prp2p/P2n3P/6P1/1P1R1PK1/4R3 b - - 5 49",
	                              "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/3R4 w - - 6 54",
	                              "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/6R1 b - - 6 59",
	                              "8/5k2/1p4p1/p1pK3p/P2n1P1P/6P1/1P6/4R3 b - - 14 63",
	                              "8/1R6/1p1K1kp1/p6p/P1p2P1P/6P1/1Pn5/8 w - - 0 67",
	                              "1rb1rn1k/p3q1bp/2p3p1/2p1p3/2P1P2N/PP1RQNP1/1B3P2/4R1K1 b - - 4 23",
	                              "4rrk1/pp1n1pp1/q5p1/P1pP4/2n3P1/7P/1P3PB1/R1BQ1RK1 w - - 3 22",
	                              "r2qr1k1/pb1nbppp/1pn1p3/2ppP3/3P4/2PB1NN1/PP3PPP/R1BQR1K1 w - - 4 12",
	                              "2r2k2/8/4P1R1/1p6/8/P4K1N/7b/2B5 b - - 0 55",
	                              "6k1/5pp1/8/2bKP2P/2P5/p4PNb/B7/8 b - - 1 44",
	                              "2rqr1k1/1p3p1p/p2p2p1/P1nPb3/2B1P3/5P2/1PQ2NPP/R1R4K w - - 3 25",
	                              "r1b2rk1/p1q1ppbp/6p1/2Q5/8/4BP2/PPP3PP/2KR1B1R b - - 2 14",
	                              "6r1/5k2/p1b1r2p/1pB1p1p1/1Pp3PP/2P1R1K1/2P2P2/3R4 w - - 1 36",
	                              "rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2",
	                              "2rr2k1/1p4bp/p1q1p1p1/4Pp1n/2PB4/1PN3P1/P3Q2P/2RR2K1 w - f6 0 20",
	                              "3br1k1/p1pn3p/1p3n2/5pNq/2P1p3/1PN3PP/P2Q1PB1/4R1K1 w - - 0 23",
	                              "2r2b2/5p2/5k2/p1r1pP2/P2pB3/1P3P2/K1P3R1/7R w - - 23 93"};

	    TimeLimit timer = TimeLimit();

	    for (auto fen : fens){
	        timer.start();
	        Board board(fen);
	        std::atomic<bool> benchAbort(false);
	        TTable TT;
	        std::unique_ptr<Search::ThreadInfo> thread = std::make_unique<Search::ThreadInfo>(ThreadType::SECONDARY, TT, benchAbort);

	        Search::Limit limit = Search::Limit();
	        limit.depth = (int64_t)BENCH_DEPTH; limit.movetime = 0; limit.ctime = 0;
	        limit.start();

	        Search::iterativeDeepening(board, *thread, limit, nullptr);

	        int ms = timer.elapsed();
	        totalMS += ms;
	        totalNodes += thread->nodes;
	        
	        std::cout << "-----------------------------------------------------------------------" << std::endl;
	        std::cout << "FEN: " << fen << std::endl;
	        std::cout << "Nodes: " << thread->nodes << std::endl;
	        std::cout << "Time: " << ms << "ms" << std::endl;
	        std::cout << "-----------------------------------------------------------------------\n" << std::endl;
	    }
	    std::cout << "Completed Benchmark" << std::endl;
	    std::cout << "Total Nodes: " << totalNodes << std::endl;
	    std::cout << "Elapsed Time: " << totalMS << "ms" << std::endl;
	    int nps = static_cast<int64_t>((totalNodes / totalMS) * 1000);
	    std::cout << "Average NPS: " << nps << std::endl;
	    std::cout << totalNodes << " nodes " << nps << " nps" << std::endl;
	}
}

void Searcher::start(Board &board, Search::Limit limit){
	mainInfo->nodes = 0;
	mainThread = std::thread(Search::iterativeDeepening, std::ref(board), std::ref(*mainInfo), limit, this);
	for (auto& info : workerInfo) {
		info.nodes = 0;
		pool->enqueue([&, &info = info] {
			Search::iterativeDeepening(board, info, limit, nullptr);
		});
	}
}

void Searcher::stop(){
	abort.store(true, std::memory_order_relaxed);
	if (mainThread.joinable())
		mainThread.join();
}

void Searcher::initialize(int threads){
	stop();
	if (pool)
		pool->stop();
	abort.store(false);
	threads -= 1;
	workerInfo.clear();
	for (int i=0;i<threads;i++){
		workerInfo.emplace_back(ThreadType::SECONDARY, TT, abort);
	}
	pool = std::make_unique<ThreadPool>(threads);
}