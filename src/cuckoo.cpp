#include "cuckoo.h"
#include "external/chess.hpp"
#include <array>
#include <bit>
#include <cassert>
#include <cstring>
#include <sstream>

using namespace chess;

namespace Cuckoo {

	void init() {
		moves.fill(Move::NO_MOVE);
		keyDiffs.fill(0);

		uint32_t count = 0;

		for (PieceType pt : {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
			for (Color c : {Color::WHITE, Color::BLACK}) {
				for (int from = 0; from < 63; from++) {
					for (int to = from + 1; to < 64; to++) {
						Bitboard pieceAttacks;
						switch (pt) {
							case int(PieceType::KNIGHT):
								pieceAttacks = attacks::knight(Square(from));
								break;
							case int(PieceType::BISHOP):
								pieceAttacks = attacks::bishop(Square(from), Bitboard(0));
								break;
							case int(PieceType::ROOK):
								pieceAttacks = attacks::rook(Square(from), Bitboard(0));
								break;
							case int(PieceType::QUEEN):
								pieceAttacks = attacks::queen(Square(from), Bitboard(0));
								break;
							case int(PieceType::KING):
								pieceAttacks = attacks::king(Square(from));
								break;
						}

						if (!pieceAttacks.check(to))
							continue;

						Move move = Move::make<Move::NORMAL>(Square(from), Square(to));
						uint64_t key = 0;
						key ^= Zobrist::piece(Piece(pt, c), Square(from));
						key ^= Zobrist::piece(Piece(pt, c), Square(to));
						key ^= Zobrist::sideToMove();

						uint32_t slot = h1(key);

						while (true) {
							std::swap(keyDiffs[slot], key);
							std::swap(moves[slot], move);

							if (move == Move::NO_MOVE)
								break;

							slot = slot == h1(key) ? h2(key) : h1(key);
						}
						count++;
					}
				}
			}
		}
	}
}