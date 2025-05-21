#pragma once

#include "external/chess.hpp"
#include "tt.h"
#include "timeman.h"
#include "nnue.h"
#include "parameters.h"
#include "util.h"
#include "eval.h"
#include <atomic>
#include <cstring>
#include <thread>
#include <algorithm>

using namespace chess;




const int MVVLVA[7][7] = {
    {15, 14, 13, 12, 11, 10, 0}, // Victim P, attacker P, N, B, R, Q, K, None
    {25, 24, 23, 22, 21, 20, 0}, // Victim N, attacker P, N, B, R, Q, K, None
    {35, 34, 33, 32, 31, 30, 0}, // Victim B, attacker P, N, B, R, Q, K, None
    {45, 44, 43, 42, 41, 40, 0}, // Victim R, attacker P, N, B, R, Q, K, None
    {55, 54, 53, 52, 51, 50, 0}, // Victim Q, attacker P, N, B, R, Q, K, None
    {0,  0,  0,  0,  0,  0,  0}  // Victim None, attacker P, N, B, R, Q, K, None
};
// Sirius values
constexpr int MVV_VALUES[6] = {800, 2400, 2400, 4800, 7200};

enum ThreadType {
    MAIN      = 1,
    SECONDARY = 0
};

struct Searcher;

namespace Search {

struct PVList {
	std::array<chess::Move, MAX_PLY> moves;
	uint32_t length;
	PVList(){
		length = 0;
	}
	void update(chess::Move move, const PVList &who){
		moves[0] = move;
		std::copy(who.moves.begin(), who.moves.begin() + who.length, moves.begin() + 1);
		length = who.length + 1;
	}
	auto &operator=(const PVList &other){
		std::copy(other.moves.begin(), other.moves.begin() + other.length, moves.begin());
		this->length = other.length;
		return *this;
	}
};


struct Stack {
    PVList pv;
    chess::Move killer;
    int    staticEval;
    int    historyScore;
    Move excluded{};
    MultiArray<int16_t, 64, 6, 2> *conthist;
};

void fillLmr();

struct ThreadInfo {
	ThreadType type;
	TTable &TT;
	std::atomic<bool> &abort;
	Board board;
	Accumulator accumulator;
	std::atomic<uint64_t> nodes;
	Move bestMove;
	int minNmpPly;
	int rootDepth;

	std::array<std::array<std::array<int, 64>, 64>, 2> history;
	// indexed by [prev stm][prev pt][prev to][stm][pt][to]
	MultiArray<int16_t, 64, 6, 2, 64, 6, 2> conthist;
	// indexed by [stm][moving pt][cap pt][to]
	MultiArray<int, 64, 6, 6, 2> capthist;
	
	MultiArray<int, PAWN_CORR_HIST_ENTRIES, 2> pawnCorrhist;

	ThreadInfo(ThreadType type, TTable &TT, std::atomic<bool> &abort) : type(type), TT(TT), abort(abort) {
		abort.store(false, std::memory_order_relaxed);
		this->board = Board();
		std::memset(&history, 0, sizeof(history));
		conthist.fill(DEFAULT_HISTORY);
		capthist.fill((int)DEFAULT_HISTORY);
		pawnCorrhist.fill((int)DEFAULT_HISTORY);
		nodes = 0;
		bestMove = Move::NO_MOVE;
		minNmpPly = 0;
		rootDepth = 0;
		//ttHits = 0;
	}
	ThreadInfo(const ThreadInfo &other) : type(other.type), TT(other.TT), abort(other.abort), history(other.history), 
											bestMove(other.bestMove), minNmpPly(other.minNmpPly), rootDepth(other.rootDepth) {
		this->board = other.board;
		conthist = other.conthist;
		nodes.store(other.nodes.load(std::memory_order_relaxed), std::memory_order_relaxed);
	}
	// History updaters
	void updateHistory(Color c, Move m, int bonus){
		int clamped = std::clamp((int)bonus, int(-MAX_HISTORY), int(MAX_HISTORY));
		history[(int)c][m.from().index()][m.to().index()] += clamped - history[(int)c][m.from().index()][m.to().index()] * std::abs(clamped) / MAX_HISTORY;
	}
	void updateCapthist(Board &board, Move m, int bonus){
		int clamped = std::clamp((int)bonus, int(-MAX_HISTORY), int(MAX_HISTORY));
		int &entry = capthist[board.sideToMove()][board.at<PieceType>(m.from())][board.at<PieceType>(m.to())][m.to().index()];
		entry += clamped - entry * std::abs(clamped) / MAX_HISTORY;
	}
	void updateConthist(Stack *ss, Board &board, Move m, int16_t bonus){
		auto updateEntry = [&](int16_t &entry) {
			int16_t clamped = std::clamp((int)bonus, int(-MAX_HISTORY), int(MAX_HISTORY));
			entry += clamped - entry * std::abs(clamped) / MAX_HISTORY;
			entry = std::clamp((int)entry, int(-MAX_HISTORY), int(MAX_HISTORY));
		};
		if ((ss-1)->conthist != nullptr)
			updateEntry(( *(ss-1)->conthist)[board.sideToMove()][(int)board.at<PieceType>(m.from())][m.to().index()] );
	}
	void updateCorrhist(Board &board, int bonus){
		int &entry = pawnCorrhist[board.sideToMove()][getPawnHashIndex(board, PAWN_CORR_HIST_ENTRIES)];
		int clamped = std::clamp(bonus, -MAX_CORR_HIST / 4, MAX_CORR_HIST / 4);
		entry += clamped - entry * std::abs(clamped) / MAX_CORR_HIST;
	}
	
	// History getters
	int getHistory(Color c, Move m){
		return history[(int)c][m.from().index()][m.to().index()];
	}
	int getCapthist(Board &board, Move m){
		return capthist[board.sideToMove()][board.at<PieceType>(m.from())][board.at<PieceType>(m.to())][m.to().index()];
	}
	MultiArray<int16_t, 64, 6, 2> *getConthistSegment(Board &board, Move m){
		return &conthist[board.sideToMove()][(int)board.at<PieceType>(m.from())][m.to().index()];
	}
	int16_t getConthist(MultiArray<int16_t, 64, 6, 2> *c, Board &board, Move m){
		assert(c != nullptr);
		return (*c)[board.sideToMove()][(int)board.at<PieceType>(m.from())][m.to().index()];
	}
	int getQuietHistory(Board &board, Move m, Stack *ss){
		int hist = getHistory(board.sideToMove(), m);
		if (ss != nullptr && (ss-1)->conthist != nullptr)
			hist += getConthist((ss-1)->conthist, board, m);
		return hist;
	}
	int correctStaticEval(Board &board, int eval){
		int pawnEntry = pawnCorrhist[board.sideToMove()][getPawnHashIndex(board, PAWN_CORR_HIST_ENTRIES)];

		int correction = 0;
		correction += PAWN_CORR_WEIGHT * pawnEntry;

		int corrected = eval + correction / 512;
		return std::clamp(corrected, GETTING_MATED + 1, FOUND_MATE - 1);
	}
	void reset(){
		nodes.store(0, std::memory_order_relaxed);
		bestMove = Move::NO_MOVE;
		for (auto &i : history)
			for (auto &j : i)
				j.fill(0);
		conthist.fill(DEFAULT_HISTORY);
		capthist.fill((int)DEFAULT_HISTORY);
		pawnCorrhist.fill((int)DEFAULT_HISTORY);
	}
};


struct Limit {
	TimeLimit timer;
	int64_t depth;
	int64_t ctime;
	int64_t movetime;
	int64_t maxnodes;
	int64_t softnodes;
	int64_t inc;
	int64_t softtime;
	bool enableClock;
	Color color;

	Limit(){
		depth = 0;
		ctime = 0;
		movetime = 0;
		maxnodes = -1;
		softnodes = -1;
		softtime = 0;
		enableClock = true;
		inc = 0;
	}
	Limit(int64_t depth, int64_t ctime, int64_t movetime, Color color) : depth(depth), ctime(ctime), movetime(movetime), color(color) {
		
	}
	// I will eventually fix this ugly code
	void start(){
		enableClock = movetime != 0 || ctime != 0;
		if (depth == 0)
			depth = MAX_PLY - 5;
		if (enableClock)
			softtime = 0;
		if (ctime != 0){
			// Calculate movetime
			// this was like ~34 lol
			movetime = ctime / (inc <= 0 ? 30 : 20) + inc / 2;
			softtime = movetime * 0.63;
			movetime -= 15;
		}
		timer.start();
	}
	bool outOfNodes(int64_t cnt){
		return maxnodes != -1 && cnt > maxnodes;
	}
	bool softNodes(int64_t cnt){
		return softnodes != -1 && cnt > softnodes;
	}
	bool outOfTime(){
		return (enableClock && static_cast<int64_t>(timer.elapsed()) >= movetime);
	}
	bool outOfTimeSoft(){
		if (!enableClock || softtime == 0)
			return false;
		return (static_cast<int64_t>(timer.elapsed()) >= softtime);
	}
};
//int search(Board &board, int depth, int ply, int alpha, int beta, Stack *ss, ThreadInfo &thread);
//int iterativeDeepening(Board board, ThreadInfo &threadInfo, Searcher *searcher);
int iterativeDeepening(Board &board, ThreadInfo &threadInfo, Limit limit, Searcher *searcher);

void bench();
} 