#pragma once

#include "external/chess.hpp"
#include <vector>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

using namespace chess;

enum TTFlag {
	EXACT = 1,
	BETA_CUT = 2,
	FAIL_LOW = 3
};

struct TTEntry {
	uint64_t zobrist;
	chess::Move move;
	int score;
	int staticEval;
	uint8_t flag;
	uint8_t depth;
	TTEntry(){
		this->zobrist = 0;
		this->move = chess::Move();
		this->score = 0;
		this->flag = 0;
		this->depth = 0;
		this->staticEval = -32767;
	}
	TTEntry(uint64_t key, chess::Move best, int score, int eval, uint8_t flag, uint8_t depth){
		this->zobrist = key;
		this->move = best;
		this->score = score;
		this->flag = flag;
		this->depth = depth;
		this->staticEval = eval;
	}
};

struct TTable {
private:
	std::vector<TTEntry> table;
public:
	uint64_t size;

	TTable(uint64_t sizeMB = 16){

		resize(sizeMB);
	}
	void clear(){
		std::fill(table.begin(), table.end(), TTEntry{});
	}
	void resize(uint64_t MB){
		size = MB * 1024 * 1024 / sizeof(TTEntry);
		if (size == 0)
			size++;
		table.resize(size);
	}
	uint64_t index(uint64_t key) { 
		return key % size;
	}

	void setEntry(TTEntry entry){
		table[index(entry.zobrist)] = entry;
	}
	TTEntry *getEntry(uint64_t key){
		return &table[index(key)];
	}

};