#include "external/chess.hpp"
#include "nnue.h"
#include "util.h"
#include <bit>
#include <vector>
#include <sstream>
#include <cassert>
#include <cstring>



Bitboard BetweenBB[64][64] = {};
Bitboard Rays[64][8] = {};
// Accumulator wrapper
void MakeMove(Board &board, Accumulator &acc, Move &move){
	PieceType to = board.at<PieceType>(move.to());
	PieceType from = board.at<PieceType>(move.from());
	Color stm = board.sideToMove();
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

// Utility attackers
Bitboard attackersTo(Board &board, Square s, Bitboard occ){
	return (attacks::pawn(Color::WHITE, s) & board.pieces(PieceType::PAWN, Color::BLACK))
		|  (attacks::pawn(Color::BLACK, s) & board.pieces(PieceType::PAWN, Color::WHITE))
		|  (attacks::rook(s, occ) & (board.pieces(PieceType::ROOK) | board.pieces(PieceType::QUEEN)) )
		|  (attacks::bishop(s, occ) & (board.pieces(PieceType::BISHOP) | board.pieces(PieceType::QUEEN)))
		|  (attacks::knight(s) & board.pieces(PieceType::KNIGHT))
		|  (attacks::king(s) & board.pieces(PieceType::KING));
}

int oppDir(int dir){
	switch (dir){
		case DirIndex::NORTH: return DirIndex::SOUTH;
		case DirIndex::SOUTH: return DirIndex::NORTH;
		case DirIndex::EAST: return DirIndex::WEST;
		case DirIndex::WEST: return DirIndex::EAST;

		case DirIndex::NORTH_EAST: return DirIndex::SOUTH_WEST;
		case DirIndex::NORTH_WEST: return DirIndex::SOUTH_EAST;
		case DirIndex::SOUTH_EAST: return DirIndex::NORTH_WEST;
		case DirIndex::SOUTH_WEST: return DirIndex::NORTH_EAST;
	}
	return -1;
}
void initLookups(){
	for (Square sq = Square::SQ_A1; sq <= Square::SQ_H8; ++sq){
		Bitboard bb = Bitboard::fromSquare(sq);
		Bitboard tmp = bb;
		Bitboard result = Bitboard(0);
		while (tmp){
			tmp = tmp << 8;
			result |= tmp;
		}
		Rays[sq.index()][DirIndex::NORTH] = result;

		tmp = bb;
		result = Bitboard(0);
		while (tmp){
			tmp = tmp >> 8;
			result |= tmp;
		}
		Rays[sq.index()][DirIndex::SOUTH] = result;

		tmp = bb;
		result = Bitboard(0);
		while (tmp){
			tmp = tmp << 1;
			tmp &= ~Bitboard(File::FILE_A);
			result |= tmp;
		}
		Rays[sq.index()][DirIndex::EAST] = result;

		tmp = bb;
		result = Bitboard(0);
		while (tmp){
			tmp = tmp >> 1;
			tmp &= ~Bitboard(File::FILE_H);
			result |= tmp;
		}
		Rays[sq.index()][DirIndex::WEST] = result;

		tmp = bb;
		result = Bitboard(0);
		while (tmp){
			tmp = tmp << 8;
			tmp = tmp << 1;
			tmp &= ~Bitboard(File::FILE_A);
			result |= tmp;
		}
		Rays[sq.index()][DirIndex::NORTH_EAST] = result;

		tmp = bb;
		result = Bitboard(0);
		while (tmp){
			tmp = tmp << 8;
			tmp = tmp >> 1;
			tmp &= ~Bitboard(File::FILE_H);
			result |= tmp;
		}
		Rays[sq.index()][DirIndex::NORTH_WEST] = result;

		tmp = bb;
		result = Bitboard(0);
		while (tmp){
			tmp = tmp >> 8;
			tmp = tmp << 1;
			tmp &= ~Bitboard(File::FILE_A);
			result |= tmp;
		}
		Rays[sq.index()][DirIndex::SOUTH_EAST] = result;

		tmp = bb;
		result = Bitboard(0);
		while (tmp){
			tmp = tmp >> 8;
			tmp = tmp >> 1;
			tmp &= ~Bitboard(File::FILE_H);
			result |= tmp;
		}
		Rays[sq.index()][DirIndex::SOUTH_WEST] = result;
	}
	// BETWEEN BB -------------------------
	for (Square sq1 = Square::SQ_A1; sq1 <= Square::SQ_H8; sq1++){
		for (Square sq2 = Square::SQ_A1; sq2 <= Square::SQ_H8; sq2++){
			if (sq1 == sq2)
				continue;
			for (int dir=0;dir<8;dir++){
				Bitboard srcRay = Rays[sq1.index()][dir];
				if (!(srcRay & Bitboard::fromSquare(sq2)).empty()){
					Bitboard dstRay = Rays[sq2.index()][oppDir(dir)];
					BetweenBB[sq1.index()][sq2.index()] = srcRay & dstRay;
					
				}
			}
		}
	}
}

// Pinners are the ~stm pieces that pin stm to the king
// I got that mixed around
void pinnersBlockers(Board &board, Color stm, StateInfo *sti){
	Bitboard pin = Bitboard(0);
	Bitboard blockers = Bitboard(0);
	Square ksq = board.kingSq(stm);
	Bitboard snipers = ((attacks::rook(ksq, Bitboard()) & (board.pieces(PieceType::QUEEN)|board.pieces(PieceType::ROOK)) )
						| (attacks::bishop(ksq, Bitboard()) & (board.pieces(PieceType::QUEEN)|board.pieces(PieceType::BISHOP))))
						& board.us(~stm);
	Bitboard occ = board.occ() ^ snipers;

	while (snipers) {
		Square sniperSq = snipers.pop();

		Bitboard b = BetweenBB[ksq.index()][sniperSq.index()] & occ;
		Bitboard moreThanOne = Bitboard(b.getBits() & (b.getBits()-1));
		if (b && moreThanOne.empty()){
			sti->kingBlockers[(int)(stm)] |= b;
			if (b & board.us(stm))
				sti->pinners[(int)(~stm)] |= Bitboard::fromSquare(sniperSq);
		}

	}	
}
// Stockfish and Sirius
bool SEE(Board &board, Move &move, int margin){
	Square from = move.from();
	Square to = move.to();
	StateInfo state = StateInfo();
	pinnersBlockers(board, Color::WHITE, &state);
	pinnersBlockers(board, Color::BLACK, &state);
	int swap = PieceValue[(int)board.at<PieceType>(to)] - margin;
	if (swap < 0)
		return false;
	swap = PieceValue[(int)board.at<PieceType>(from)] - swap;
	if (swap <= 0)
		return true;

	Bitboard occupied = board.occ() ^ Bitboard::fromSquare(from) ^ Bitboard::fromSquare(to);
	Color stm = board.sideToMove();
	Bitboard attackers = attackersTo(board, to, occupied);
	Bitboard stmAttackers = Bitboard(); 
	Bitboard bb = Bitboard();
	int res = 1;
	while (true){
		stm = ~stm;
		attackers &= occupied;
		
		if (!(stmAttackers = attackers & board.us(stm)))
			break;
		if ( !(state.pinners[(int)(~stm)] & occupied).empty() ){
			stmAttackers &= ~state.kingBlockers[(int)(stm)];
			if (stmAttackers.empty())
				break;
		}
		res ^= 1;

		if ((bb = stmAttackers & board.pieces(PieceType::PAWN))){
			if ((swap = PawnValue - swap) < res)
				break;
			occupied ^= Bitboard::fromSquare(bb.lsb());
			attackers |= attacks::bishop(to, occupied) & (board.pieces(PieceType::BISHOP) | board.pieces(PieceType::QUEEN));
		}
		else if ((bb = stmAttackers & board.pieces(PieceType::KNIGHT))){
			if ((swap = KnightValue - swap) < res)
				break;
			occupied ^= Bitboard::fromSquare(bb.lsb());
		}
		else if ((bb = stmAttackers & board.pieces(PieceType::BISHOP))){
			if ((swap = BishopValue - swap) < res)
				break;
			occupied ^= Bitboard::fromSquare(bb.lsb());
			attackers |= attacks::bishop(to, occupied) & (board.pieces(PieceType::BISHOP) | board.pieces(PieceType::QUEEN));
		}
		else if ((bb = stmAttackers & board.pieces(PieceType::ROOK))){
			if ((swap = RookValue - swap) < res)
				break;
			occupied ^= Bitboard::fromSquare(bb.lsb());
			attackers |= attacks::rook(to, occupied) & (board.pieces(PieceType::ROOK) | board.pieces(PieceType::QUEEN));
		}
		else if ((bb = stmAttackers & board.pieces(PieceType::QUEEN))){
			swap = QueenValue - swap;
			occupied ^= Bitboard::fromSquare(bb.lsb());
			attackers |= attacks::bishop(to, occupied) & (board.pieces(PieceType::BISHOP) | board.pieces(PieceType::QUEEN));
			attackers |= attacks::rook(to, occupied) & (board.pieces(PieceType::ROOK) | board.pieces(PieceType::QUEEN));
		}
		else 
			return (attackers & ~board.us(stm)).empty() ? res : res^1;
	}
	return bool(res);
}