#include "util.h"
#include "external/chess.hpp"
#include "nnue.h"
#include <bit>
#include <cassert>
#include <cstring>
#include <sstream>
#include <vector>

Bitboard BetweenBB[64][64] = {};
Bitboard Rays[64][8] = {};

// Pawn Hash reset
uint64_t resetPawnHash(Board& board) {
    uint64_t key = 0ULL;
    Bitboard occ = board.pieces(PieceType::PAWN);
    while (occ) {
        Square sq = occ.pop();
        key ^= Zobrist::piece(board.at(sq), sq);
    }
    return key;
}
// Nonpawn Hash
uint64_t resetNonPawnHash(Board& board, Color c) {
    uint64_t key = 0ULL;
    Bitboard occ = board.us(c) ^ board.pieces(PieceType::PAWN, c);
    while (occ) {
        Square sq = occ.pop();
        key ^= Zobrist::piece(board.at(sq), sq);
    }
    return key;
}
uint64_t resetMajorHash(Board& board) {
    uint64_t key = 0ULL;
    Bitboard occ = board.pieces(PieceType::ROOK) |
                   board.pieces(PieceType::QUEEN) |
                   board.pieces(PieceType::KING);
    while (occ) {
        Square sq = occ.pop();
        key ^= Zobrist::piece(board.at(sq), sq);
    }
    return key;
}

uint64_t resetMinorHash(Board& board) {
    uint64_t key = 0ULL;
    Bitboard occ = board.pieces(PieceType::KNIGHT) |
                   board.pieces(PieceType::BISHOP) |
                   board.pieces(PieceType::KING);
    while (occ) {
        Square sq = occ.pop();
        key ^= Zobrist::piece(board.at(sq), sq);
    }
    return key;
}

bool isMajor(PieceType pt) {
    return pt == PieceType::ROOK || pt == PieceType::QUEEN ||
           pt == PieceType::KING;
}

bool isMinor(PieceType pt) {
    return pt == PieceType::KNIGHT || pt == PieceType::BISHOP ||
           pt == PieceType::KING;
}

// Pseudo Legal Check
bool isLegal(Board& board, Move move) {
    if (moveIsNull(move))
        return false;

    Color stm = board.sideToMove();
    Piece srcPiece = board.at(move.from());

    if (srcPiece == Piece::NONE || srcPiece.color() != stm)
        return false;

    Piece dstPiece = board.at(move.to());
    PieceType srcPieceType = srcPiece.type();
    PieceType dstPieceType = dstPiece.type();

    // Lazy Solution
    // Make this sane later
    Movelist legals;
    movegen::legalmoves<movegen::MoveGenType::ALL>(legals, board,
                                                   1 << (int)srcPieceType);
    if (std::find(legals.begin(), legals.end(), move) == legals.end())
        return false;
    return true;
}

// Utility attackers
Bitboard attackersTo(Board& board, Square s, Bitboard occ) {
    return (attacks::pawn(Color::WHITE, s) &
            board.pieces(PieceType::PAWN, Color::BLACK)) |
           (attacks::pawn(Color::BLACK, s) &
            board.pieces(PieceType::PAWN, Color::WHITE)) |
           (attacks::rook(s, occ) &
            (board.pieces(PieceType::ROOK) | board.pieces(PieceType::QUEEN))) |
           (attacks::bishop(s, occ) & (board.pieces(PieceType::BISHOP) |
                                       board.pieces(PieceType::QUEEN))) |
           (attacks::knight(s) & board.pieces(PieceType::KNIGHT)) |
           (attacks::king(s) & board.pieces(PieceType::KING));
}

int oppDir(int dir) {
    switch (dir) {
        case DirIndex::NORTH:
            return DirIndex::SOUTH;
        case DirIndex::SOUTH:
            return DirIndex::NORTH;
        case DirIndex::EAST:
            return DirIndex::WEST;
        case DirIndex::WEST:
            return DirIndex::EAST;

        case DirIndex::NORTH_EAST:
            return DirIndex::SOUTH_WEST;
        case DirIndex::NORTH_WEST:
            return DirIndex::SOUTH_EAST;
        case DirIndex::SOUTH_EAST:
            return DirIndex::NORTH_WEST;
        case DirIndex::SOUTH_WEST:
            return DirIndex::NORTH_EAST;
    }
    return -1;
}
void initLookups() {
    for (Square sq = Square::SQ_A1; sq <= Square::SQ_H8; ++sq) {
        Bitboard bb = Bitboard::fromSquare(sq);
        Bitboard tmp = bb;
        Bitboard result = Bitboard(0);
        while (tmp) {
            tmp = tmp << 8;
            result |= tmp;
        }
        Rays[sq.index()][DirIndex::NORTH] = result;

        tmp = bb;
        result = Bitboard(0);
        while (tmp) {
            tmp = tmp >> 8;
            result |= tmp;
        }
        Rays[sq.index()][DirIndex::SOUTH] = result;

        tmp = bb;
        result = Bitboard(0);
        while (tmp) {
            tmp = tmp << 1;
            tmp &= ~Bitboard(File::FILE_A);
            result |= tmp;
        }
        Rays[sq.index()][DirIndex::EAST] = result;

        tmp = bb;
        result = Bitboard(0);
        while (tmp) {
            tmp = tmp >> 1;
            tmp &= ~Bitboard(File::FILE_H);
            result |= tmp;
        }
        Rays[sq.index()][DirIndex::WEST] = result;

        tmp = bb;
        result = Bitboard(0);
        while (tmp) {
            tmp = tmp << 8;
            tmp = tmp << 1;
            tmp &= ~Bitboard(File::FILE_A);
            result |= tmp;
        }
        Rays[sq.index()][DirIndex::NORTH_EAST] = result;

        tmp = bb;
        result = Bitboard(0);
        while (tmp) {
            tmp = tmp << 8;
            tmp = tmp >> 1;
            tmp &= ~Bitboard(File::FILE_H);
            result |= tmp;
        }
        Rays[sq.index()][DirIndex::NORTH_WEST] = result;

        tmp = bb;
        result = Bitboard(0);
        while (tmp) {
            tmp = tmp >> 8;
            tmp = tmp << 1;
            tmp &= ~Bitboard(File::FILE_A);
            result |= tmp;
        }
        Rays[sq.index()][DirIndex::SOUTH_EAST] = result;

        tmp = bb;
        result = Bitboard(0);
        while (tmp) {
            tmp = tmp >> 8;
            tmp = tmp >> 1;
            tmp &= ~Bitboard(File::FILE_H);
            result |= tmp;
        }
        Rays[sq.index()][DirIndex::SOUTH_WEST] = result;
    }
    // BETWEEN BB -------------------------
    for (Square sq1 = Square::SQ_A1; sq1 <= Square::SQ_H8; sq1++) {
        for (Square sq2 = Square::SQ_A1; sq2 <= Square::SQ_H8; sq2++) {
            if (sq1 == sq2)
                continue;
            for (int dir = 0; dir < 8; dir++) {
                Bitboard srcRay = Rays[sq1.index()][dir];
                if (!(srcRay & Bitboard::fromSquare(sq2)).empty()) {
                    Bitboard dstRay = Rays[sq2.index()][oppDir(dir)];
                    BetweenBB[sq1.index()][sq2.index()] = srcRay & dstRay;
                }
            }
        }
    }
}

// Pinners are the ~stm pieces that pin stm to the king
// I got that mixed around
void pinnersBlockers(Board& board, Color stm, StateInfo* sti) {
    Bitboard pin = Bitboard(0);
    Bitboard blockers = Bitboard(0);
    Square ksq = board.kingSq(stm);
    Bitboard snipers =
        ((attacks::rook(ksq, Bitboard()) &
          (board.pieces(PieceType::QUEEN) | board.pieces(PieceType::ROOK))) |
         (attacks::bishop(ksq, Bitboard()) &
          (board.pieces(PieceType::QUEEN) | board.pieces(PieceType::BISHOP)))) &
        board.us(~stm);
    Bitboard occ = board.occ() ^ snipers;

    while (snipers) {
        Square sniperSq = snipers.pop();

        Bitboard b = BetweenBB[ksq.index()][sniperSq.index()] & occ;
        Bitboard moreThanOne = Bitboard(b.getBits() & (b.getBits() - 1));
        if (b && moreThanOne.empty()) {
            sti->kingBlockers[(int)(stm)] |= b;
            if (b & board.us(stm))
                sti->pinners[(int)(~stm)] |= Bitboard::fromSquare(sniperSq);
        }
    }
}
// Stockfish and Sirius
bool SEE(Board& board, Move& move, int margin) {

    if (move.typeOf() != Move::NORMAL)
        return 0 >= margin;

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

    Bitboard occupied =
        board.occ() ^ Bitboard::fromSquare(from) ^ Bitboard::fromSquare(to);
    Color stm = board.sideToMove();
    Bitboard attackers = attackersTo(board, to, occupied);
    Bitboard stmAttackers = Bitboard();
    Bitboard bb = Bitboard();
    int res = 1;
    while (true) {
        stm = ~stm;
        attackers &= occupied;

        if (!(stmAttackers = attackers & board.us(stm)))
            break;
        if (!(state.pinners[(int)(~stm)] & occupied).empty()) {
            stmAttackers &= ~state.kingBlockers[(int)(stm)];
            if (stmAttackers.empty())
                break;
        }
        res ^= 1;

        if ((bb = stmAttackers & board.pieces(PieceType::PAWN))) {
            if ((swap = PawnValue - swap) < res)
                break;
            occupied ^= Bitboard::fromSquare(bb.lsb());
            attackers |= attacks::bishop(to, occupied) &
                         (board.pieces(PieceType::BISHOP) |
                          board.pieces(PieceType::QUEEN));
        } else if ((bb = stmAttackers & board.pieces(PieceType::KNIGHT))) {
            if ((swap = KnightValue - swap) < res)
                break;
            occupied ^= Bitboard::fromSquare(bb.lsb());
        } else if ((bb = stmAttackers & board.pieces(PieceType::BISHOP))) {
            if ((swap = BishopValue - swap) < res)
                break;
            occupied ^= Bitboard::fromSquare(bb.lsb());
            attackers |= attacks::bishop(to, occupied) &
                         (board.pieces(PieceType::BISHOP) |
                          board.pieces(PieceType::QUEEN));
        } else if ((bb = stmAttackers & board.pieces(PieceType::ROOK))) {
            if ((swap = RookValue - swap) < res)
                break;
            occupied ^= Bitboard::fromSquare(bb.lsb());
            attackers |=
                attacks::rook(to, occupied) & (board.pieces(PieceType::ROOK) |
                                               board.pieces(PieceType::QUEEN));
        } else if ((bb = stmAttackers & board.pieces(PieceType::QUEEN))) {
            swap = QueenValue - swap;
            occupied ^= Bitboard::fromSquare(bb.lsb());
            attackers |= attacks::bishop(to, occupied) &
                         (board.pieces(PieceType::BISHOP) |
                          board.pieces(PieceType::QUEEN));
            attackers |=
                attacks::rook(to, occupied) & (board.pieces(PieceType::ROOK) |
                                               board.pieces(PieceType::QUEEN));
        } else
            return (attackers & ~board.us(stm)).empty() ? res : res ^ 1;
    }
    return bool(res);
}