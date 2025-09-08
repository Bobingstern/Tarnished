#include "search.h"
#include "eval.h"
#include "movepicker.h"
#include "parameters.h"
#include "searcher.h"
#include "tt.h"
#include "util.h"
#include <algorithm>
#include <array>
#include <random>


using namespace chess;

uint8_t TT_GENERATION_COUNTER = 0;

// Accumulator wrappers
// Update the accumulators incrementally and track the
// pawn zobrist key for correction history (stored in the search stack)
// The incremental hash logic is terrible and code is ugly. I will refactor into an add piece and remove piece
// eventually
void MakeMove(Board& board, Move move, Search::Stack* ss) {
    PieceType to = board.at<PieceType>(move.to());
    PieceType from = board.at<PieceType>(move.from());
    Square epSq = board.enpassantSq();
    Color stm = board.sideToMove();

    // Hash key incremental updates
    (ss + 1)->pawnKey = ss->pawnKey;
    (ss + 1)->majorKey = ss->majorKey;
    (ss + 1)->minorKey = ss->minorKey;
    (ss + 1)->nonPawnKey[0] = ss->nonPawnKey[0];
    (ss + 1)->nonPawnKey[1] = ss->nonPawnKey[1];
    // Accumulator copy
    (ss + 1)->accumulator = ss->accumulator;
    if (move == Move::NULL_MOVE) {
        board.makeNullMove();
        return;
    }

    if (from == PieceType::PAWN) {
        // Update pawn zobrist key
        // Remove from sq pawn
        (ss + 1)->pawnKey ^= Zobrist::piece(board.at(move.from()), move.from());
        if (move.typeOf() == Move::ENPASSANT) {
            // Remove en passant pawn
            (ss + 1)->pawnKey ^= Zobrist::piece(Piece(PieceType::PAWN, ~stm), move.to().ep_square());
        } else if (to == PieceType::PAWN) {
            // Remove to sq pawn
            (ss + 1)->pawnKey ^= Zobrist::piece(board.at(move.to()), move.to());
        } else if (to != PieceType::NONE) {
            (ss + 1)->nonPawnKey[~stm] ^= Zobrist::piece(board.at(move.to()), move.to());
            if (isMajor(to))
                (ss + 1)->majorKey ^= Zobrist::piece(board.at(move.to()), move.to());
            if (isMinor(to))
                (ss + 1)->minorKey ^= Zobrist::piece(board.at(move.to()), move.to());
        }
        // Add to sq pawn
        if (move.typeOf() != Move::PROMOTION)
            (ss + 1)->pawnKey ^= Zobrist::piece(board.at(move.from()), move.to());
        else {
            (ss + 1)->nonPawnKey[stm] ^= Zobrist::piece(Piece(move.promotionType(), stm), move.to());
            if (isMajor(move.promotionType()))
                (ss + 1)->majorKey ^= Zobrist::piece(Piece(move.promotionType(), stm), move.to());
            if (isMinor(move.promotionType()))
                (ss + 1)->minorKey ^= Zobrist::piece(Piece(move.promotionType(), stm), move.to());
        }
    } else {

        if (move.typeOf() != Move::CASTLING) {
            // Remove from sq piece
            (ss + 1)->nonPawnKey[stm] ^= Zobrist::piece(board.at(move.from()), move.from());
            // Add the moving piece at the to sq
            (ss + 1)->nonPawnKey[stm] ^= Zobrist::piece(board.at(move.from()), move.to());

            if (isMajor(from)) {
                // Remove from, add to
                (ss + 1)->majorKey ^= Zobrist::piece(board.at(move.from()), move.from());
                (ss + 1)->majorKey ^= Zobrist::piece(board.at(move.from()), move.to());
            }
            if (isMinor(from)) {
                // Remove from, add to
                (ss + 1)->minorKey ^= Zobrist::piece(board.at(move.from()), move.from());
                (ss + 1)->minorKey ^= Zobrist::piece(board.at(move.from()), move.to());
            }
            // If capture remove to
            if (to == PieceType::PAWN) {
                (ss + 1)->pawnKey ^= Zobrist::piece(board.at(move.to()), move.to());
            } else if (to != PieceType::NONE) {
                (ss + 1)->nonPawnKey[~stm] ^= Zobrist::piece(board.at(move.to()), move.to());
                if (isMajor(to))
                    (ss + 1)->majorKey ^= Zobrist::piece(board.at(move.to()), move.to());
                if (isMinor(to))
                    (ss + 1)->minorKey ^= Zobrist::piece(board.at(move.to()), move.to());
            }
        }
    }

    board.makeMove(move);

    if (from == PieceType::KING)
        if (Accumulator::needRefresh(move, stm)){
            (ss + 1)->accumulator.refresh(board);
            return;
        }

    if (move.typeOf() == Move::ENPASSANT) {
        (ss + 1)->accumulator.quiet(board, stm, move.to(), from, move.from(), from);
        (ss + 1)->accumulator.subPiece(board, ~stm, move.to().ep_square(), PieceType::PAWN);
    } else if (move.typeOf() == Move::PROMOTION) {
        (ss + 1)->accumulator.subPiece(board, stm, move.from(), from);
        (ss + 1)->accumulator.addPiece(board, stm, move.to(), move.promotionType());
        if (to != PieceType::NONE)
            (ss + 1)->accumulator.subPiece(board, ~stm, move.to(), to);
    } else if (move.typeOf() == Move::CASTLING) {

        Square king = move.from();
        Square standardKing = stm == Color::WHITE ? Square::SQ_E1 : Square::SQ_E8; // For chess960
        Square kingTo = (king > move.to()) ? standardKing - 2 : standardKing + 2;
        Square rookTo = (king > move.to()) ? kingTo + 1 : kingTo - 1;

        // Remove the king from sq
        (ss + 1)->minorKey ^= Zobrist::piece(Piece(PieceType::KING, stm), king);
        (ss + 1)->majorKey ^= Zobrist::piece(Piece(PieceType::KING, stm), king);
        (ss + 1)->nonPawnKey[stm] ^= Zobrist::piece(Piece(PieceType::KING, stm), king);
        // Add him to the to sq
        (ss + 1)->minorKey ^= Zobrist::piece(Piece(PieceType::KING, stm), kingTo);
        (ss + 1)->majorKey ^= Zobrist::piece(Piece(PieceType::KING, stm), kingTo);
        (ss + 1)->nonPawnKey[stm] ^= Zobrist::piece(Piece(PieceType::KING, stm), kingTo);

        // Remove the rook
        (ss + 1)->majorKey ^= Zobrist::piece(Piece(PieceType::ROOK, stm), move.to());
        (ss + 1)->nonPawnKey[stm] ^= Zobrist::piece(Piece(PieceType::ROOK, stm), move.to());
        // Add the rook
        (ss + 1)->majorKey ^= Zobrist::piece(Piece(PieceType::ROOK, stm), rookTo);
        (ss + 1)->nonPawnKey[stm] ^= Zobrist::piece(Piece(PieceType::ROOK, stm), rookTo);

        // There are basically just 2 quiet moves now for the accumulator
        // Move king and move rook
        // Since moves are encoded as king takes rook, its very easy
        (ss + 1)->accumulator.quiet(board, stm, kingTo, PieceType::KING, move.from(), PieceType::KING);
        (ss + 1)->accumulator.quiet(board, stm, rookTo, PieceType::ROOK, move.to(), PieceType::ROOK);
    } else if (to != PieceType::NONE) {
        (ss + 1)->accumulator.capture(board, stm, move.to(), from, move.from(), from, move.to(), to);
    } else
        (ss + 1)->accumulator.quiet(board, stm, move.to(), from, move.from(), from);

}

void UnmakeMove(Board& board, Move move) {
    if (move == Move::NULL_MOVE) {
        board.unmakeNullMove();
        return;
    }
    board.unmakeMove(move);
}

namespace Search {
    std::array<std::array<std::array<int, 219>, MAX_PLY + 1>, 2> lmrTable;

    bool isWin(int score) {
        return score >= FOUND_MATE;
    }
    bool isLoss(int score) {
        return score <= GETTING_MATED;
    }
    bool isMateScore(int score) {
        return std::abs(score) >= FOUND_MATE;
    }
    int evaluate(Board& board, Accumulator& accumulator) {
        int materialOffset = 100 * board.pieces(PieceType::PAWN).count() + 450 * board.pieces(PieceType::KNIGHT).count() + 
                            450 * board.pieces(PieceType::BISHOP).count() + 650 * board.pieces(PieceType::ROOK).count() + 
                            1250 * board.pieces(PieceType::QUEEN).count();

        int eval = network.inference(board, accumulator);

        eval = eval * (26500 + materialOffset) / 32768; // Calvin yoink
        return std::clamp(eval, GETTING_MATED + 1, FOUND_MATE - 1);
    }
    void fillLmr() {
        // https://www.chessprogramming.org/Late_Move_Reductions
        // Maybe its possible to optimize a + log(x)log(y)/b with some gradient descent tuning method
        for (int isQuiet = 0; isQuiet <= 1; isQuiet++) {
            for (size_t depth = 0; depth <= MAX_PLY; depth++) {
                for (int movecount = 0; movecount <= 218; movecount++) {
                    if (depth == 0 || movecount == 0) {
                        lmrTable[isQuiet][depth][movecount] = 0;
                        continue;
                    }
                    if (isQuiet) {
                        lmrTable[isQuiet][depth][movecount] =
                            static_cast<int>((LMR_BASE_QUIET() / 100.0 + std::log(static_cast<double>(depth)) *
                                                                             std::log(static_cast<double>(movecount)) /
                                                                             (LMR_DIVISOR_QUIET() / 100.0)));
                    } else {
                        lmrTable[isQuiet][depth][movecount] =
                            static_cast<int>((LMR_BASE_NOISY() / 100.0 + std::log(static_cast<double>(depth)) *
                                                                             std::log(static_cast<double>(movecount)) /
                                                                             (LMR_DIVISOR_NOISY() / 100.0)));
                    }
                }
            }
        }
    }
    template <bool isPV> int qsearch(int ply, int alpha, const int beta, Stack* ss, ThreadInfo& thread, Limit& limit) {
        if (thread.type == ThreadType::MAIN &&
            ((thread.loadNodes() & 2047) == 0 && limit.outOfTime() || limit.outOfNodes(thread.loadNodes()))) {
            thread.searcher->stopSearching();
        }
        if (thread.stopped.load() || thread.exiting.load() || ply >= MAX_PLY - 1) {
            return (ply >= MAX_PLY - 1 && !thread.board.inCheck()) ? network.inference(thread.board, ss->accumulator)
                                                                   : 0;
        }

        ss->ply = ply;

        ProbedTTEntry ttData = {};
        bool ttHit = false;

        ttHit = thread.TT.probe(thread.board.hash(), ply, ttData);
        
        bool ttPV = isPV || (ttHit && ttData.pv);

        if (!isPV && ttData.score != EVAL_NONE &&
            (ttData.bound == TTFlag::EXACT || (ttData.bound == TTFlag::BETA_CUT && ttData.score >= beta) ||
             (ttData.bound == TTFlag::FAIL_LOW && ttData.score <= alpha))) {
            return ttData.score;
        }

        bool inCheck = thread.board.inCheck();
        int rawStaticEval, eval = 0;

        // Get the corrected static eval if not in check
        if (inCheck) {
            rawStaticEval = -EVAL_INF;
            eval = -EVAL_INF;
        } else {
            rawStaticEval = ttHit && ttData.staticEval != EVAL_NONE && !isMateScore(ttData.staticEval)
                                ? ttData.staticEval
                                : evaluate(thread.board, ss->accumulator);
            eval = thread.correctStaticEval(ss, thread.board, rawStaticEval);
        }

        if (eval >= beta)
            return eval;
        if (eval > alpha)
            alpha = eval;

        int bestScore = eval;
        int moveCount = 0;
        Move qBestMove = Move::NO_MOVE;
        uint8_t ttFlag = TTFlag::FAIL_LOW;

        // Calculuate Threats
        ss->threats = calculateThreats(thread.board);
        
        // This will do evasions as well
        Move move;
        MovePicker picker = MovePicker(&thread, ss, ttData.move, true);

        while (!moveIsNull(move = picker.nextMove())) {
            if (thread.stopped.load() || thread.exiting.load())
                return bestScore;

            // SEE Pruning
            if (bestScore > GETTING_MATED && !SEE(thread.board, move, QS_SEE_MARGIN()))
                continue;

            thread.TT.prefetch(prefetchKey(thread.board, move));

            MakeMove(thread.board, move, ss);

            thread.nodes.fetch_add(1, std::memory_order::relaxed);
            moveCount++;
            int score = -qsearch<isPV>(ply + 1, -beta, -alpha, ss + 1, thread, limit);

            UnmakeMove(thread.board, move);

            if (score > bestScore) {
                bestScore = score;
                if (score > alpha) {
                    alpha = score;
                    qBestMove = move;
                }
            }
            if (score >= beta) {
                ttFlag = TTFlag::BETA_CUT;
                break;
            }
        }
        if (!moveCount && inCheck)
            return -MATE + ply;

        thread.TT.store(thread.board.hash(), qBestMove, bestScore, rawStaticEval, ttFlag, 0, ply, ttPV);

        return bestScore;
    }
    template <bool isPV>
    int search(int depth, int ply, int alpha, int beta, bool cutnode, Stack* ss, ThreadInfo& thread, Limit& limit) {
        // bool isPV = alpha != beta - 1;
        bool root = ply == 0;
        ss->ply = ply;

        if (isPV)
            ss->pv.length = 0;
        if (depth <= 0) {
            return qsearch<isPV>(ply, alpha, beta, ss, thread, limit);
        }
        // Terminal Conditions (and checkmate)
        if (!root) {
            if (thread.board.isRepetition(1) || thread.board.isHalfMoveDraw())
                return 0;

            if (thread.type == ThreadType::MAIN &&
                ((thread.loadNodes() & 2047) == 0 && limit.outOfTime() || limit.outOfNodes(thread.loadNodes())) &&
                thread.rootDepth != 1) {
                thread.searcher->stopSearching();
            }
            if (thread.stopped.load() || thread.exiting.load() || ply >= MAX_PLY - 1) {
                return (ply >= MAX_PLY - 1 && !thread.board.inCheck())
                           ? network.inference(thread.board, ss->accumulator)
                           : 0;
            }
        }

        ProbedTTEntry ttData = {};
        bool ttHit = false;

        if (moveIsNull(ss->excluded)) {
            ttHit = thread.TT.probe(thread.board.hash(), ply, ttData);
        }

        bool ttPV = isPV || (ttHit && ttData.pv);

        if (!isPV && ttHit && ttData.depth >= depth &&
            (ttData.bound == TTFlag::EXACT || (ttData.bound == TTFlag::BETA_CUT && ttData.score >= beta) ||
             (ttData.bound == TTFlag::FAIL_LOW && ttData.score <= alpha))) {
            return ttData.score;
        }

        int bestScore = -EVAL_INF;
        int oldAlpha = alpha;
        int rawStaticEval = EVAL_NONE;
        int score = bestScore;
        int moveCount = 0;
        bool inCheck = thread.board.inCheck();
        ss->conthist = nullptr;
        ss->eval = EVAL_NONE;
        (ss + 1)->failHighs = 0;

        // Get the corrected static evaluation if we're not in singular search or check
        int corrplexity = 0;
        if (inCheck) {
            ss->staticEval = EVAL_NONE;
        } else if (!moveIsNull(ss->excluded)) {
            rawStaticEval = ss->eval = ss->staticEval;
        } else {
            rawStaticEval = ttHit && ttData.staticEval != EVAL_NONE && !isMateScore(ttData.staticEval)
                                ? ttData.staticEval
                                : evaluate(thread.board, ss->accumulator);
            ss->eval = ss->staticEval = thread.correctStaticEval(ss, thread.board, rawStaticEval);
            corrplexity = rawStaticEval - ss->staticEval;
        }
        // Improving heurstic
        // We are better than 2 plies ago
        bool improving =
            !inCheck && ply > 1 && (ss - 2)->staticEval != EVAL_NONE && (ss - 2)->staticEval < ss->staticEval;
        uint8_t ttFlag = TTFlag::FAIL_LOW;

        // Pruning
        if (!root && !isPV && !inCheck && moveIsNull(ss->excluded)) {
            // Reverse Futility Pruning
            int rfpMargin = RFP_SCALE() * (depth - improving);
            rfpMargin += corrplexity * RFP_CORRPLEXITY_SCALE() / 128;

            if (depth <= 8 && ss->eval - rfpMargin >= beta)
                return ss->eval;

            if (depth <= 4 && std::abs(alpha) < 2000 && ss->staticEval + RAZORING_SCALE() * depth <= alpha) {
                int score = qsearch<isPV>(ply, alpha, alpha + 1, ss, thread, limit);
                if (score <= alpha)
                    return score;
            }

            // Null Move Pruning
            Bitboard nonPawns = thread.board.us(thread.board.sideToMove()) ^
                                thread.board.pieces(PieceType::PAWN, thread.board.sideToMove());
            if (depth >= 2 && ss->eval >= beta && ply > thread.minNmpPly && !nonPawns.empty()) {
                // Sirius formula
                const int reduction = NMP_BASE_REDUCTION() + depth / NMP_REDUCTION_SCALE() +
                                      std::min(2, (ss->eval - beta) / NMP_EVAL_SCALE());

                ss->conthist = nullptr;
                ss->contCorrhist = nullptr;

                // Null move prefetch is just flip color
                thread.TT.prefetch(thread.board.hash() ^ Zobrist::sideToMove());

                MakeMove(thread.board, Move(Move::NULL_MOVE), ss);
                int nmpScore =
                    -search<false>(depth - reduction, ply + 1, -beta, -beta + 1, !cutnode, ss + 1, thread, limit);
                UnmakeMove(thread.board, Move(Move::NULL_MOVE));

                if (nmpScore >= beta) {
                    // Zugzwang verifiction
                    // All "real" moves are bad, so doing a null causes a cutoff
                    // do a reduced search to verify and if that also fails high
                    // then all is well, else dont prune
                    if (depth <= 15 || thread.minNmpPly > 0)
                        return isWin(nmpScore) ? beta : nmpScore;
                    thread.minNmpPly = ply + (depth - reduction) * 3 / 4;
                    int verification =
                        search<false>(depth - NMP_BASE_REDUCTION(), ply + 1, beta - 1, beta, true, ss, thread, limit);
                    thread.minNmpPly = 0;
                    if (verification >= beta)
                        return verification;
                }
            }
        }

        // Internal Iterative Reduction
        if (depth >= 3 && moveIsNull(ss->excluded) && (isPV || cutnode) && (!ttData.move || ttData.depth + 3 < depth))
            depth--;

        // Thought
        // What if we arrange a vector C = {....} of weights and input of say {alpha, beta, eval...}
        // and use some sort of data generation method to create a pruning heuristic
        // with something like sigmoid(C dot I) >= 0.75 ?

        // Calculuate Threats
        ss->threats = calculateThreats(thread.board);

        Move bestMove = Move::NO_MOVE;
        Move move;
        MovePicker picker = MovePicker(&thread, ss, ttData.move, false);

        Movelist seenQuiets;
        Movelist seenCaptures;

        bool skipQuiets = false;

        while (!moveIsNull(move = picker.nextMove())) {
            if (thread.stopped.load() || thread.exiting.load())
                return bestScore;

            bool isQuiet = !thread.board.isCapture(move);
            if (move == ss->excluded)
                continue;
            if (isQuiet && skipQuiets)
                continue;
            if (isQuiet)
                seenQuiets.add(move);
            else
                seenCaptures.add(move);

            ss->historyScore =
                isQuiet ? thread.getQuietHistory(thread.board, move, ss) : thread.getCapthist(thread.board, move, ss);

            int baseLMR = LMR_BASE_SCALE() * lmrTable[isQuiet && move.typeOf() != Move::PROMOTION][depth][moveCount];

            if (!root && bestScore > GETTING_MATED) {
                int lmrDepth = std::max(depth - baseLMR / 1024, 0);
                // Late Move Pruning
                if (!isPV && !inCheck && moveCount >= 2 + depth * depth / (2 - improving))
                    break;

                if (!isPV && isQuiet && depth <= 4 && thread.getQuietHistory(thread.board, move, ss) <= -HIST_PRUNING_SCALE() * depth) {
                    skipQuiets = true;
                    continue;
                }

                int futility = ss->staticEval + FP_SCALE() * depth + FP_OFFSET() + ss->historyScore / FP_HIST_DIVISOR();
                if (!inCheck && isQuiet && lmrDepth <= 8 && std::abs(alpha) < 2000 && futility <= alpha) {
                    skipQuiets = true;
                    continue;
                }
                // Reckless idea 
                // Bad noisy futility pruning
                futility = ss->staticEval + BNFP_DEPTH_SCALE() * depth + BNFP_MOVECOUNT_SCALE() * moveCount / 128;
                if (!inCheck && depth <= 5 && picker.stage == MPStage::BAD_NOISY && std::abs(alpha) < 2000 && futility <= alpha) {
                    if (!isMateScore(bestScore) && bestScore <= futility)
                        bestScore = futility;
                    break;
                }

                int seeMargin = isQuiet ? SEE_QUIET_SCALE() * lmrDepth : SEE_NOISY_SCALE() * lmrDepth;
                if (!SEE(thread.board, move, seeMargin))
                    continue;

            }

            // Singular Extensions
            // Sirius conditions
            // https://github.com/mcthouacbb/Sirius/blob/15501c19650f53f0a10973695a6d284bc243bf7d/Sirius/src/search.cpp#L620
            bool doSE = !root && moveIsNull(ss->excluded) && depth >= 7 && Move(ttData.move) == move &&
                        ttData.depth >= depth - 3 && ttData.bound != TTFlag::FAIL_LOW && !isMateScore(ttData.score);

            int extension = 0;

            if (doSE) {
                int sBeta = std::max(-MATE, ttData.score - SE_BETA_SCALE() * depth / 16);
                int sDepth = (depth - 1) / 2;
                // How good are we without this move
                ss->excluded = Move(ttData.move);
                int seScore = search<false>(sDepth, ply + 1, sBeta - 1, sBeta, cutnode, ss, thread, limit);
                ss->excluded = Move::NO_MOVE;

                if (seScore < sBeta) {
                    if (!isPV && seScore < sBeta - SE_DOUBLE_MARGIN())
                        extension = 2; // Double extension
                    else
                        extension = 1; // Singular Extension

                    depth += (extension > 1 && depth < 14);
                } 
                else if (sBeta >= beta)
                    return sBeta;
                else if (ttData.score >= beta)
                    extension = -3; // Negative Extension
                else if (cutnode)
                    extension = -2;

            }

            // Update Continuation History
            ss->move = move;
            ss->movedPiece = thread.board.at<PieceType>(move.from());
            ss->conthist = thread.getConthistSegment(thread.board, move);
            ss->contCorrhist = thread.getContCorrhistSegment(thread.board, move);

            uint64_t previousNodes = thread.loadNodes();

            thread.TT.prefetch(prefetchKey(thread.board, move));

            MakeMove(thread.board, move, ss);
            moveCount++;
            thread.nodes.fetch_add(1, std::memory_order::relaxed);

            int newDepth = depth - 1 + extension;
            // Late Move Reduction
            if (depth >= 3 && moveCount > 2 + root) {
                int reduction = baseLMR;

                // Factorized "inference"
                // ---------------------------------------------------------------
                // | We take a set of input features and arrange 3 tables for up |
                // | to 3 way interactions between them. For example, a two way  |
                // | interaction would be two_way_table[i] * (x && y), three     |
                // | way would be three_way_table[j] * (x && y && z) etc         |
                // | For example 6 variables, that gives us a one way           |
                // | table of 6, two table of 6x5/2=15, and three way of         |
                // | 6x5x3/3!=20. Thanks to AGE for this idea                    |
                // ---------------------------------------------------------------
                reduction += lmrConvolution({isQuiet, !isPV, improving, cutnode, ttPV, ttHit, ((ss + 1)->failHighs > 2), corrplexity > LMR_CORR_MARGIN()});
                // Reduce less if good history
                reduction -= 1024 * ss->historyScore / LMR_HIST_DIVISOR();
                
                reduction /= 1024;

                int lmrDepth = std::min(newDepth, std::max(1, newDepth - reduction));

                score = -search<false>(lmrDepth, ply + 1, -alpha - 1, -alpha, true, ss + 1, thread, limit);
                // Re-search at normal depth
                if (score > alpha && lmrDepth < newDepth) {
                    bool doDeeper = score > bestScore + LMR_DEEPER_BASE() + LMR_DEEPER_SCALE() * newDepth;
                    bool doShallower = score < bestScore + newDepth;

                    newDepth += doDeeper;
                    newDepth -= doShallower;

                    score = -search<false>(newDepth, ply + 1, -alpha - 1, -alpha, !cutnode, ss + 1, thread, limit);
                }
            } else if (!isPV || moveCount > 1) {
                score = -search<false>(newDepth, ply + 1, -alpha - 1, -alpha, !cutnode, ss + 1, thread, limit);
            }
            if (isPV && (moveCount == 1 || score > alpha)) {
                score = -search<isPV>(newDepth, ply + 1, -beta, -alpha, false, ss + 1, thread, limit);
            }
            UnmakeMove(thread.board, move);

            if (root && thread.type == ThreadType::MAIN)
                limit.updateNodes(move, thread.loadNodes() - previousNodes);

            if (score > bestScore) {
                bestScore = score;
                if (score > alpha) {
                    bestMove = move;
                    ss->bestMove = bestMove;
                    ttFlag = TTFlag::EXACT;
                    alpha = score;
                    if (isPV) {
                        ss->pv.update(move, (ss + 1)->pv);
                    }
                }
            }
            if (score >= beta) {
                ttFlag = TTFlag::BETA_CUT;
                ss->killer = isQuiet ? bestMove : Move::NO_MOVE;
                ss->failHighs++;
                // Butterfly History
                // Continuation History
                // Capture History
                int bonus = historyBonus(depth);
                int malus = historyMalus(depth);
                if (isQuiet) {
                    thread.updateQuietHistory(ss, move, bonus);
                    for (const Move quietMove : seenQuiets) {
                        if (quietMove == move)
                            continue;
                        thread.updateQuietHistory(ss, quietMove, malus);
                    }
                } else {
                    thread.updateCapthist(ss, thread.board, move, bonus);
                }
                // Always malus captures
                for (const Move noisyMove : seenCaptures) {
                    if (noisyMove == move)
                        continue;
                    thread.updateCapthist(ss, thread.board, noisyMove, malus);
                }
                break;
            }
        }
        if (!moveCount) {
            if (!moveIsNull(ss->excluded))
                return alpha;
            return inCheck ? -MATE + ply : 0;
        }
        // Sf fail firm idea
        if (bestScore >= beta && !isMateScore(bestScore) && !isMateScore(alpha))
            bestScore = (bestScore * depth + beta) / (depth + 1);

        if (moveIsNull(ss->excluded)) {
            // Update correction history
            bool isBestQuiet = !thread.board.isCapture(bestMove);
            if (!inCheck && (isBestQuiet || moveIsNull(bestMove)) &&
                (ttFlag == TTFlag::EXACT || ttFlag == TTFlag::BETA_CUT && bestScore > ss->staticEval ||
                 ttFlag == TTFlag::FAIL_LOW && bestScore < ss->staticEval)) {
                int bonus =
                    static_cast<int>((CORRHIST_BONUS_WEIGHT() / 100.0) * (bestScore - ss->staticEval) * depth / 8);
                thread.updateCorrhist(ss, thread.board, bonus);
            }

            // Update TT
            thread.TT.store(thread.board.hash(), bestMove, bestScore, rawStaticEval, ttFlag, depth, ply, ttPV);
        }

        return bestScore;
    }

    int iterativeDeepening(Board board, ThreadInfo& threadInfo, Limit limit, Searcher* searcher) {
        threadInfo.board = board;
        Accumulator baseAcc;
        baseAcc.refresh(threadInfo.board);

        bool isMain = threadInfo.type == ThreadType::MAIN;

        auto stack = std::make_unique<std::array<Stack, MAX_PLY + 6 + 3>>();
        Stack* ss = reinterpret_cast<Stack*>(stack->data() + 3); // Saftey for conthist
        std::memset(stack.get(), 0, sizeof(Stack) * (MAX_PLY + 6 + 3));

        PVList lastPV{};
        int score = -EVAL_INF;
        int lastScore = -EVAL_INF;

        int64_t avgnps = 0;
        for (int depth = 1; depth <= limit.depth; depth++) {
            auto aborted = [&](bool canSoft) {
                if (threadInfo.stopped.load())
                    return true;
                // Only check soft node limit outside of aspiration
                if (isMain)
                    return limit.outOfTime() || limit.outOfNodes(threadInfo.nodes) || (limit.softNodes(threadInfo.nodes) && canSoft);
                else
                    return limit.softNodes(threadInfo.nodes) && canSoft;
            };
            threadInfo.rootDepth = depth;
            ss->pawnKey = resetPawnHash(threadInfo.board);
            ss->majorKey = resetMajorHash(threadInfo.board);
            ss->minorKey = resetMinorHash(threadInfo.board);
            ss->nonPawnKey[0] = resetNonPawnHash(threadInfo.board, Color::WHITE);
            ss->nonPawnKey[1] = resetNonPawnHash(threadInfo.board, Color::BLACK);
            ss->accumulator = baseAcc;
            int eval = evaluate(threadInfo.board, ss->accumulator);

            if (limit.softNodes(threadInfo.nodes)){
                break;
            }
            // Aspiration Windows
            if (depth >= MIN_ASP_WINDOW_DEPTH()) {
                int delta = INITIAL_ASP_WINDOW();
                int alpha = std::max(lastScore - delta, -EVAL_INF);
                int beta = std::min(lastScore + delta, EVAL_INF);
                int aspDepth = depth;
                while (!aborted(false)) {
                    score = search<true>(std::max(aspDepth, 1), 0, alpha, beta, false, ss, threadInfo, limit);
                    if (score <= alpha) {
                        beta = (alpha + beta) / 2;
                        alpha = std::max(alpha - delta, -EVAL_INF);
                        aspDepth = depth;
                    } else {
                        if (score >= beta) {
                            beta = std::min(beta + delta, EVAL_INF);
                            aspDepth = std::max(aspDepth - 1, depth - 5);
                        } else
                            break;
                    }
                    delta += delta * ASP_WIDENING_FACTOR() / 16;
                }
            } else
                score = search<true>(depth, 0, -EVAL_INF, EVAL_INF, false, ss, threadInfo, limit);
            // ---------------------
            if (depth != 1 && aborted(false)) {
                break;
            }

            lastScore = score;
            lastPV = ss->pv;

            // Save best scores
            threadInfo.completed = depth;
            threadInfo.bestMove = ss->bestMove;
            threadInfo.bestRootScore = score;

            if (!isMain) {
                continue;
            }

            // Reporting
            uint64_t nodecnt = searcher->nodeCount();

            std::stringstream pvss;           // String stream for the mainline
            Board pvBoard = threadInfo.board; // Test board for WDL and eval normalization since we need the final board
                                              // state of the mainline
            for (int i = 0; i < lastPV.length; i++) {
                pvss << uci::moveToUci(lastPV.moves[i], pvBoard.chess960()) << " ";
                pvBoard.makeMove(lastPV.moves[i]);
            }
            if (searcher->printInfo) {
                if (!PRETTY_PRINT) {
                    std::cout << "info depth " << depth << " score ";
                    if (score >= FOUND_MATE || score <= GETTING_MATED) {
                        std::cout << "mate " << ((score < 0) ? "-" : "") << (MATE - std::abs(score)) / 2 + 1;
                    } else {
                        int s = searcher->normalizeEval ? scaleEval(score, threadInfo.board) : score; // Only scale if WDL enabled
                        std::cout << "cp " << s;
                        if (searcher->showWDL) {
                            WDL wdl = computeWDL(score, threadInfo.board);
                            std::cout << " wdl " << wdl.w << " " << wdl.d << " " << wdl.l;
                        }
                    }
                    std::cout << " hashfull " << searcher->TT.hashfull();
                    std::cout << " nodes " << nodecnt << " nps " << nodecnt / (limit.timer.elapsed() + 1) * 1000 << " time " << limit.timer.elapsed() << " pv ";
                    std::cout << pvss.str() << std::endl;
                }
                else {
                    CURSOR::clearAll();
                    std::cout << "\n";
                    std::vector<Move> moves;
                    for (int i = 0; i < std::min((int)lastPV.length, getTerminalWidth() / 23 - 1); i++) {
                        moves.push_back(lastPV.moves[i]);
                    }
                    printBoard(threadInfo.board, moves);

                    int normEval = scaleEval(score, threadInfo.board);
                    WDL wdl = computeWDL(score, threadInfo.board);
                    Color stm = threadInfo.board.sideToMove();

                    std::cout << COLORS::GREY << "Hash size:  " << COLORS::WHITE << searcher->TT.mbSize << "MB" << std::endl;
                    std::cout << COLORS::GREY << "Hash usage: " << COLORS::WHITE << searcher->TT.hashfull() / 10.0 << "%\n" << std::endl;

                    std::cout << COLORS::GREY << "Nodes:            " << COLORS::WHITE << nodecnt << std::endl;
                    std::cout << COLORS::GREY << "Nodes per second: " << COLORS::WHITE << nodecnt / (limit.timer.elapsed() + 1) * 1000 << std::endl;
                    std::cout << COLORS::GREY << "Time:             " << COLORS::WHITE << (limit.timer.elapsed() / 1000.0) << "s " << std::endl;
                    std::cout << COLORS::GREY << "Depth:            " << COLORS::WHITE << depth << "\n" << std::endl;

                    if (score >= FOUND_MATE || score <= GETTING_MATED) {
                        std::cout << COLORS::GREY << "Score:     ";
                        std::cout << (score < 0 ? COLORS::RED : COLORS::GREEN);
                        std::cout << ((score < 0) ? "#-" : "#") << (MATE - std::abs(score)) / 2 + 1 << std::endl;
                    }
                    else {
                        RGB scoreRGB = scoreToRGB(normEval);
                        RGB wRGB = wdlRGB(wdl.w, wdl.d, wdl.l);
                        RGB dRGB = wdlRGB(wdl.d, wdl.w, wdl.l);
                        RGB lRGB = wdlRGB(wdl.l, wdl.d, wdl.w);

                        std::cout << COLORS::GREY << "Score:     ";
                        fmt::print(fg(fmt::rgb(scoreRGB.r, scoreRGB.g, scoreRGB.b)), "{:.2f}", normEval / 100.0);

                        std::cout << COLORS::GREY << " [" ;
                        fmt::print(fg(fmt::rgb(wRGB.r, wRGB.g, wRGB.b)), "{:.2f}%", wdl.w / 10.0);
                        std::cout << COLORS::GREY << " W | ";

                        fmt::print(fg(fmt::rgb(dRGB.r, dRGB.g, dRGB.b)), "{:.2f}%", wdl.d / 10.0);
                        std::cout << COLORS::GREY << " D | ";

                        fmt::print(fg(fmt::rgb(lRGB.r, lRGB.g, lRGB.b)), "{:.2f}%", wdl.l / 10.0);
                        std::cout << COLORS::GREY << " L]\n";

                    }
                    
                    std::cout << COLORS::GREY << "Best Move: " << COLORS::WHITE << uci::moveToUci(threadInfo.bestMove, searcher->board.chess960()) << "\n" << std::endl;
                    std::cout << COLORS::GREY << "Main Line: " << COLORS::WHITE << pvss.str() << std::endl;
                }
            }
            // Time control (soft)
            double complexity = 0;
            if (!isMateScore(score))
                complexity = (COMPLEXITY_TM_SCALE() / 100.0) * std::abs(eval - score) * std::log(static_cast<double>(depth));
            if (limit.outOfTimeSoft(lastPV.moves[0], threadInfo.nodes, complexity))
                break;
        }

        return lastScore;
    }

}