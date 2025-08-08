#pragma once

#include "eval.h"
#include "external/chess.hpp"
#include "nnue.h"
#include "parameters.h"
#include "timeman.h"
#include "tt.h"
#include "util.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

using namespace chess;

// Sirius values
constexpr int MVV_VALUES[6] = {800, 2400, 2400, 4800, 7200};

enum ThreadType { MAIN = 1, SECONDARY = 0 };

struct Searcher;

namespace Search {

    void fillLmr();
    bool isMateScore(int score);
    bool isWin(int score);
    bool isLoss(int score);
    int storeScore(int score, int ply);
    int readScore(int score, int ply);
    int evaluate(Board& board, Accumulator& accumulator);

    inline int historyBonus(int depth) {
        return std::min(HIST_BONUS_QUADRATIC() * depth * depth + HIST_BONUS_LINEAR() * depth - HIST_BONUS_OFFSET(),
                        2048);
    }
    inline int historyMalus(int depth) {
        return -std::min(HIST_MALUS_QUADRATIC() * depth * depth + HIST_MALUS_LINEAR() * depth + HIST_MALUS_OFFSET(),
                         1024);
    }

    inline uint64_t prefetchKey(Board& board, Move move) {
        Piece movingPiece = board.at(move.from());
        Piece captured = board.at(move.to());
        uint64_t key = board.hash();

        // Update Zobrist
        key ^= Zobrist::piece(movingPiece, move.from());
        key ^= Zobrist::piece(movingPiece, move.to());

        if (captured != Piece::NONE)
            key ^= Zobrist::piece(captured, move.to());

        key ^= Zobrist::sideToMove();
        return key;

    }

    struct PVList {
            std::array<chess::Move, MAX_PLY> moves;
            uint32_t length;
            PVList() {
                length = 0;
            }
            void update(chess::Move move, const PVList& who) {
                moves[0] = move;
                std::copy(who.moves.begin(), who.moves.begin() + who.length, moves.begin() + 1);
                length = who.length + 1;
            }
            auto& operator=(const PVList& other) {
                std::copy(other.moves.begin(), other.moves.begin() + other.length, moves.begin());
                this->length = other.length;
                return *this;
            }
    };

    struct Stack {
            PVList pv;
            chess::Move killer;
            int staticEval;
            int eval;
            int historyScore;
            int ply;
            int failHighs;
            int rfpCorrection;

            uint64_t pawnKey;
            uint64_t majorKey;
            uint64_t minorKey;
            std::array<uint64_t, 2> nonPawnKey;

            std::array<Bitboard, 7> threats;

            Move excluded{};
            Move bestMove{};

            Move move{};
            PieceType movedPiece;
            MultiArray<int16_t, 2, 6, 64>* conthist;
            MultiArray<int16_t, 2, 6, 64>* contCorrhist;

            Accumulator accumulator;
            std::array<bool, LMR_ONE_COUNT> lmrFeatures = {0};
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

            std::array<uint64_t, 4096> nodeCounts;

            Limit() {
                depth = 0;
                ctime = 0;
                movetime = 0;
                maxnodes = -1;
                softnodes = -1;
                softtime = 0;
                enableClock = true;
                inc = 0;
            }
            Limit(int64_t depth, int64_t ctime, int64_t movetime, Color color)
                : depth(depth), ctime(ctime), movetime(movetime), color(color) {}
            // I will eventually fix this ugly code
            void start() {
                enableClock = movetime != 0 || ctime != 0;
                if (depth == 0)
                    depth = MAX_PLY - 5;
                if (enableClock)
                    softtime = 0;
                if (ctime != 0) {
                    // Calculate movetime and softime
                    movetime = ctime * 0.5 - 50;
                    softtime = 0.6 * (ctime / 20 + inc * 3 / 4);
                }
                timer.start();
            }
            void updateNodes(Move move, uint64_t nodes) {
                nodeCounts[move.move() & 4095] += nodes;
            }
            bool outOfNodes(int64_t cnt) {
                return maxnodes != -1 && cnt > maxnodes;
            }
            bool softNodes(int64_t cnt) {
                return softnodes != -1 && cnt > softnodes;
            }
            bool outOfTime() {
                return (enableClock && static_cast<int64_t>(timer.elapsed()) >= movetime);
            }
            bool outOfTimeSoft(Move bestMove, uint64_t totalNodes, double complexity) {
                if (!enableClock || softtime == 0)
                    return false;

                double prop = static_cast<double>(nodeCounts[bestMove.move() & 4095]) / static_cast<double>(totalNodes);
                double scale = (NODE_TM_BASE() / 100.0 - prop) * (NODE_TM_SCALE() / 100.0);
                double compScale = std::max((COMPLEXITY_TM_BASE() / 100.0) + std::clamp(complexity, 0.0, 200.0) / static_cast<double>(COMPLEXITY_TM_DIVISOR()), 1.0);
                return (static_cast<int64_t>(timer.elapsed()) >= softtime * scale * compScale);
            }
    };

    struct alignas(128) ThreadInfo {
            std::thread thread;
            ThreadType type;
            TTable& TT;

            std::mutex mutex;
            std::condition_variable cv;


            std::atomic<bool> searching;;
            std::atomic<bool> stopped = false;
            std::atomic<bool> exiting = false;

            Board board;
            Limit limit;
            Accumulator accumulator;
            std::atomic<uint64_t> nodes;

            Move bestMove;
            int bestRootScore;
            int minNmpPly;
            int rootDepth;
            int completed;

            Searcher* searcher;
            int threadId;

            // indexed by [stm][from][to][threat]
            MultiArray<int, 2, 64, 64, 4> history;
            // indexed by [stm][from][to]
            MultiArray<int16_t, 2, 64, 64> rfpCorrection;
            // indexed by [stm][hash % entries][pt][to]
            MultiArray<int16_t, 2, PAWN_HIST_ENTRIES, 6, 64> pawnHistory;
            // indexed by [prev stm][prev pt][prev to][stm][pt][to]
            MultiArray<int16_t, 2, 6, 64, 2, 6, 64> conthist;
            MultiArray<int16_t, 2, 6, 64, 2, 6, 64> contCorrhist;
            // indexed by [stm][moving pt][cap pt][to][threat]
            MultiArray<int, 2, 6, 6, 64, 4> capthist;
            // indexed by [stm][hash % entries]
            MultiArray<int16_t, 2, CORR_HIST_ENTRIES> pawnCorrhist;
            MultiArray<int16_t, 2, CORR_HIST_ENTRIES> majorCorrhist;
            MultiArray<int16_t, 2, CORR_HIST_ENTRIES> minorCorrhist;
            MultiArray<int16_t, 2, CORR_HIST_ENTRIES> whiteNonPawnCorrhist;
            MultiArray<int16_t, 2, CORR_HIST_ENTRIES> blackNonPawnCorrhist;

            ThreadInfo(ThreadType t, TTable& tt, Searcher* s);
            ThreadInfo(int id, TTable& tt, Searcher* s);
            ThreadInfo(const ThreadInfo& other)
                : type(other.type), TT(other.TT), history(other.history), bestMove(other.bestMove),
                  minNmpPly(other.minNmpPly), rootDepth(other.rootDepth), bestRootScore(other.bestRootScore) {
                this->board = other.board;
                nodes.store(other.nodes.load());
                conthist = other.conthist;
                pawnHistory = other.pawnHistory;
                contCorrhist = other.contCorrhist;
                capthist = other.capthist;
                pawnCorrhist = other.pawnCorrhist;
                majorCorrhist = other.majorCorrhist;
                minorCorrhist = other.minorCorrhist;
                whiteNonPawnCorrhist = other.whiteNonPawnCorrhist;
                blackNonPawnCorrhist = other.blackNonPawnCorrhist;
            }
            void exit();
            void startSearching();
            void waitForSearchFinished();
            void idle();
            size_t loadNodes() {
                return nodes.load(std::memory_order::relaxed);
            }

            int threatIndex(Move move, Bitboard threats){
                return 2 * threats.check(move.from().index()) + threats.check(move.to().index());
            }

            // --------------- History updaters ---------------------
            // Make use of the history gravity formula:
            // v += bonus - v * abs(bonus) / max_hist
            // bonus is clamped with max_hist and max_hist/4 for correction history

            // Butterfly history
            void updateHistory(Stack* ss, Board& board, Move m, int bonus) {
                int clamped = std::clamp(int(bonus), int(-MAX_HISTORY), int(MAX_HISTORY));
                int& entry = history[(int)board.sideToMove()][m.from().index()][m.to().index()][threatIndex(m, ss->threats[6])];
                entry += clamped - entry * std::abs(clamped) / MAX_HISTORY;
            }

            // Capture History
            void updateCapthist(Stack* ss, Board& board, Move m, int bonus) {
                int clamped = std::clamp(int(bonus), int(-MAX_HISTORY), int(MAX_HISTORY));
                int& entry = capthist[board.sideToMove()][board.at<PieceType>(m.from())][board.at<PieceType>(m.to())]
                                     [m.to().index()][threatIndex(m, ss->threats[6])];
                entry += clamped - entry * std::abs(clamped) / MAX_HISTORY;
            }

            // Continuation History
            void updateConthist(Stack* ss, Board& board, Move m, int16_t bonus) {
                auto updateEntry = [&](int16_t& entry) {
                    int16_t clamped = std::clamp((int)bonus, int(-MAX_HISTORY), int(MAX_HISTORY));
                    entry += clamped - entry * std::abs(clamped) / MAX_HISTORY;
                    entry = std::clamp(int(entry), int(-MAX_HISTORY), int(MAX_HISTORY));
                };
                if (ss->ply > 0 && (ss - 1)->conthist != nullptr)
                    updateEntry(
                        (*(ss - 1)->conthist)[board.sideToMove()][(int)board.at<PieceType>(m.from())][m.to().index()]);
                if (ss->ply > 1 && (ss - 2)->conthist != nullptr)
                    updateEntry(
                        (*(ss - 2)->conthist)[board.sideToMove()][(int)board.at<PieceType>(m.from())][m.to().index()]);
            }

            // Pawn History
            void updatePawnhist(Stack* ss, Board& board, Move m, int16_t bonus) {
                int16_t clamped = std::clamp((int)bonus, int(-MAX_HISTORY), int(MAX_HISTORY));
                int16_t& entry = 
                    pawnHistory[board.sideToMove()][ss->pawnKey % PAWN_HIST_ENTRIES][(int)board.at<PieceType>(m.from())][m.to().index()];
                entry += clamped - entry * std::abs(clamped) / MAX_HISTORY;
                entry = std::clamp(int(entry), int(-MAX_HISTORY), int(MAX_HISTORY));
            }

            void updateQuietHistory(Stack* ss, Move m, int bonus) {
                updateHistory(ss, board, m, bonus);
                updateConthist(ss, board, m, int16_t(bonus));
                updatePawnhist(ss, board, m, int16_t(bonus));
            }

            // Update RFP Correction
            void updateRfpCorrection(Move m, int bonus) {
                auto updateEntry = [&](int16_t& entry) {
                    int16_t clamped = std::clamp(bonus, -MAX_CORR_HIST / 6, MAX_CORR_HIST / 6);
                    entry += clamped - entry * std::abs(clamped) / MAX_CORR_HIST;
                };
                updateEntry(rfpCorrection[board.sideToMove()][m.from().index()][m.to().index()]);
            }

            // Static eval correction history
            void updateCorrhist(Stack* ss, Board& board, int bonus) {
                auto updateEntry = [&](int16_t& entry) {
                    int16_t clamped = std::clamp(bonus, -MAX_CORR_HIST / 4, MAX_CORR_HIST / 4);
                    entry += clamped - entry * std::abs(clamped) / MAX_CORR_HIST;
                };
                updateEntry(pawnCorrhist[board.sideToMove()][ss->pawnKey % CORR_HIST_ENTRIES]);
                updateEntry(majorCorrhist[board.sideToMove()][ss->majorKey % CORR_HIST_ENTRIES]);
                updateEntry(minorCorrhist[board.sideToMove()][ss->minorKey % CORR_HIST_ENTRIES]);
                updateEntry(whiteNonPawnCorrhist[board.sideToMove()][ss->nonPawnKey[0] % CORR_HIST_ENTRIES]);
                updateEntry(blackNonPawnCorrhist[board.sideToMove()][ss->nonPawnKey[1] % CORR_HIST_ENTRIES]);
                // Continuation Correction History
                if (ss->ply >= 2 && (ss - 2)->contCorrhist != nullptr && !moveIsNull((ss - 2)->move) && !moveIsNull((ss - 1)->move)) {
                    auto &table = *(ss - 2)->contCorrhist;
                    updateEntry(
                        table[~board.sideToMove()][(int)((ss - 1)->movedPiece)][(ss - 1)->move.to().index()]
                    );
                }
            }
            // ----------------- History getters
            int getHistory(Color c, Move m, Stack* ss) {
                return history[(int)c][m.from().index()][m.to().index()][threatIndex(m, ss->threats[6])];
            }

            int getCapthist(Board& board, Move m, Stack* ss) {
                return capthist[board.sideToMove()][board.at<PieceType>(m.from())][board.at<PieceType>(m.to())]
                               [m.to().index()][threatIndex(m, ss->threats[6])];
            }

            MultiArray<int16_t, 2, 6, 64>* getConthistSegment(Board& board, Move m) {
                return &conthist[board.sideToMove()][(int)board.at<PieceType>(m.from())][m.to().index()];
            }
            MultiArray<int16_t, 2, 6, 64>* getContCorrhistSegment(Board& board, Move m) {
                return &contCorrhist[board.sideToMove()][(int)board.at<PieceType>(m.from())][m.to().index()];
            }

            int16_t getConthist(MultiArray<int16_t, 2, 6, 64>* c, Board& board, Move m) {
                assert(c != nullptr);
                return (*c)[board.sideToMove()][(int)board.at<PieceType>(m.from())][m.to().index()];
            }

            int16_t getPawnhist(Board& board, Move m, Stack* ss) {
                return pawnHistory[board.sideToMove()][ss->pawnKey % PAWN_HIST_ENTRIES][(int)board.at<PieceType>(m.from())][m.to().index()];
            }

            int getQuietHistory(Board& board, Move m, Stack* ss) {
                int hist = getHistory(board.sideToMove(), m, ss);
                hist += getPawnhist(board, m, ss);
                if (ss != nullptr && ss->ply > 0 && (ss - 1)->conthist != nullptr)
                    hist += getConthist((ss - 1)->conthist, board, m);
                if (ss != nullptr && ss->ply > 1 && (ss - 2)->conthist != nullptr)
                    hist += getConthist((ss - 2)->conthist, board, m);
                return hist;
            }

            int getRfpCorrection(Board& board, Move m) {
                return rfpCorrection[board.sideToMove()][m.from().index()][m.to().index()] / 16;
            }

            int correctStaticEval(Stack* ss, Board& board, int eval) {
                int correction = 0;
                correction += PAWN_CORR_WEIGHT() * pawnCorrhist[board.sideToMove()][ss->pawnKey % CORR_HIST_ENTRIES];
                correction += MAJOR_CORR_WEIGHT() * majorCorrhist[board.sideToMove()][ss->majorKey % CORR_HIST_ENTRIES];
                correction += MINOR_CORR_WEIGHT() * minorCorrhist[board.sideToMove()][ss->minorKey % CORR_HIST_ENTRIES];
                correction += NON_PAWN_STM_CORR_WEIGHT() *
                              whiteNonPawnCorrhist[board.sideToMove()][ss->nonPawnKey[0] % CORR_HIST_ENTRIES];
                correction += NON_PAWN_NSTM_CORR_WEIGHT() *
                              blackNonPawnCorrhist[board.sideToMove()][ss->nonPawnKey[1] % CORR_HIST_ENTRIES];

                // Continuation Correction History
                if (ss->ply >= 2 && (ss - 2)->contCorrhist != nullptr && !moveIsNull((ss - 2)->move) && !moveIsNull((ss - 1)->move)) {
                    auto &table = *(ss - 2)->contCorrhist;
                    correction += 
                        CONT_CORR_WEIGHT() * table[~board.sideToMove()][(int)((ss - 1)->movedPiece)][(ss - 1)->move.to().index()];
                }

                int corrected = eval + correction / 2048;
                return std::clamp(corrected, GETTING_MATED + 1, FOUND_MATE - 1);
            }
            void reset() {
                nodes = 0;
                bestMove = Move::NO_MOVE;
                history.fill((int)DEFAULT_HISTORY);
                conthist.fill(DEFAULT_HISTORY);
                pawnHistory.fill(DEFAULT_HISTORY);
                contCorrhist.fill(DEFAULT_HISTORY);
                capthist.fill((int)DEFAULT_HISTORY);
                pawnCorrhist.fill(DEFAULT_HISTORY);
                majorCorrhist.fill(DEFAULT_HISTORY);
                minorCorrhist.fill(DEFAULT_HISTORY);
                whiteNonPawnCorrhist.fill(DEFAULT_HISTORY);
                blackNonPawnCorrhist.fill(DEFAULT_HISTORY);
                bestRootScore = -INFINITE;
                rootDepth = 0;
                completed = 0;
            }
    };

    int iterativeDeepening(Board board, ThreadInfo& threadInfo, Limit limit, Searcher* searcher);
}