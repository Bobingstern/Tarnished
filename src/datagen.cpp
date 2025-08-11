#include "datagen.h"
#include "eval.h"
#include "nnue.h"
#include "search.h"
#include "searcher.h"
#include "timeman.h"
#include "tt.h"
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>
#include <vector>
#include <iostream>
#include <string>

using namespace chess;

MarlinFormat::MarlinFormat(Board& board) {
    const uint8_t stm = board.sideToMove() == Color::BLACK ? (1 << 7) : 0;
    occupancy = board.occ().getBits();
    epSquare = stm | (board.enpassantSq() == Square::NO_SQ ? 64 : (board.enpassantSq() ^ 7).index());
    halfmove = (uint8_t)board.halfMoveClock();
    fullmove = (uint16_t)board.fullMoveNumber();
    eval = 0;
    wdl = 0;
    extra = 0;

    Bitboard occ = board.occ();
    size_t index = 0;

    while (occ) {
        Square sq = occ.pop();
        PieceType pt = board.at<PieceType>(sq);
        Color ptColor = board.at(sq).color();

        Board::CastlingRights cr = board.castlingRights();
        Square whiteKingRook = Square(cr.getRookFile(Color::WHITE, Board::CastlingRights::Side::KING_SIDE), Rank::RANK_1);
        Square blackKingRook = Square(cr.getRookFile(Color::BLACK, Board::CastlingRights::Side::KING_SIDE), Rank::RANK_8);
        Square whiteQueenRook = Square(cr.getRookFile(Color::WHITE, Board::CastlingRights::Side::QUEEN_SIDE), Rank::RANK_1);
        Square blackQueenRook = Square(cr.getRookFile(Color::BLACK, Board::CastlingRights::Side::QUEEN_SIDE), Rank::RANK_8);
        
        if (pt == PieceType::ROOK &&
            ((sq == whiteQueenRook && ptColor == Color::WHITE && cr.has(Color::WHITE, Board::CastlingRights::Side::QUEEN_SIDE)) ||
             (sq == whiteKingRook && ptColor == Color::WHITE && cr.has(Color::WHITE, Board::CastlingRights::Side::KING_SIDE)) ||
             (sq == blackQueenRook && ptColor == Color::BLACK && cr.has(Color::BLACK, Board::CastlingRights::Side::QUEEN_SIDE)) ||
             (sq == blackKingRook && ptColor == Color::BLACK && cr.has(Color::BLACK, Board::CastlingRights::Side::KING_SIDE)))){
            pt = PieceType::NONE; // This is bad but everything will have a
                                  // piecetype cuz occupancy only, therefore
                                  // NONE is unique on this condition only
        }

        int pti = (int)pt;
        pti |= (Bitboard::fromSquare(sq) & board.us(Color::WHITE)).empty() ? 1ULL << 3 : 0;
        pieces.set(index++, pti);
    }
}

struct GameEntry {
        std::string fen;
        std::vector<Move> moves;
        std::vector<int> scores;
        double wdl;
        GameEntry(std::string f, std::vector<Move> m, std::vector<int> s, double w) {
            fen = f;
            moves = m;
            scores = s;
            wdl = w;
        }
};

void makeRandomMove(Board& board) {
    Movelist moves;
    movegen::legalmoves(moves, board);

    std::random_device rd;
    std::mt19937_64 engine(rd());
    std::uniform_int_distribution<int> dist(0, moves.size() - 1);

    board.makeMove(moves[dist(engine)]);
}

void writeBuffer(std::ofstream& outFile, GameEntry& game) {
    Board test = Board(game.fen);
    for (int i = 0; i < game.moves.size(); i++) {
        test.makeMove(game.moves[i]);
        outFile << test.getFen() << " | " << game.scores[i] << " | " << game.wdl << std::endl;
    }
    outFile.flush();
}

void writeViriformat(std::ofstream& outFile, ViriEntry& game) {
    int32_t nullbytes = 0;
    outFile.write(reinterpret_cast<const char*>(&game.header), sizeof(game.header));
    outFile.write(reinterpret_cast<const char*>(game.scores.data()), sizeof(ScoredMove) * game.scores.size());
    outFile.write(reinterpret_cast<const char*>(&nullbytes), 4);
    outFile.flush();
}

uint16_t packMove(Move m) {
    uint16_t move = 0;
    uint16_t from = (m.from()).index();
    uint16_t to = (m.to()).index();
    uint16_t flag = 0;

    if (m.typeOf() == Move::CASTLING) {
        flag = 0b10'00'000000'000000;
    } else if (m.typeOf() == Move::ENPASSANT) {
        flag = 0b01'00'000000'000000;
    } else if (m.typeOf() == Move::PROMOTION) {
        flag = 0b11'00'000000'000000 | (((uint16_t)m.promotionType() - 1) << 12);
    }
    move |= (uint16_t)(from);
    move |= (uint16_t)(to) << 6;
    move |= flag;
    return move;
}

// Ordered A-H columns
std::string randomDFRC() {
    std::random_device rd;
    std::mt19937_64 engine(rd());
    std::uniform_int_distribution<int> dist(0, 959);
    std::string black = frcFens[dist(engine)];
    std::string white = frcFens[dist(engine)];
    // Make them all uppercase for white
    std::transform(white.begin(), white.end(), white.begin(), [](unsigned char c) { return std::toupper(c); });

    std::string fen = black + "/pppppppp/8/8/8/8/PPPPPPPP/" + white + " w KQkq - 0 1";
    return fen;
}

void runThread(int ti, bool isDFRC) {
    std::string filePath = "data/nnue_thread" + std::to_string(ti) + ".vf";

    if (!std::filesystem::is_directory("data/"))
        std::filesystem::create_directory("data/");
    std::ofstream outFile(filePath, std::ios::app | std::ios::binary);

    int64_t cached = 0;
    std::vector<ScoredMove> moveScoreBuffer;
    std::vector<ViriEntry> gameBuffer;

    TTable TT;
    std::unique_ptr<Search::ThreadInfo> thread =
        std::make_unique<Search::ThreadInfo>(ThreadType::SECONDARY, TT, nullptr);
    thread->stopped = false;
    moveScoreBuffer.reserve(256);
    gameBuffer.clear();
    // Play a game
    int64_t poses = 0;
    TimeLimit timer;
    timer.start();
    for (int G = 1; G < 100'000'000; G++) {
        Board board;
        thread->reset();
        TT.clear();
        moveScoreBuffer.clear();

        if (isDFRC) {
            std::string fen = randomDFRC();
            board.set960(true);
            board.setFen(fen);
        }

        for (size_t i = 0; i < DATAGEN_RANDOM_MOVES; i++) {
            makeRandomMove(board);
            if (board.isGameOver().second != GameResult::NONE)
                break;
        }

        MarlinFormat marlin = MarlinFormat(board);
        std::string startingFen = board.getFen();
        std::pair<GameResultReason, GameResult> end = board.isGameOver();

        while (end.second == GameResult::NONE) {
            Search::Limit limit = Search::Limit();
            limit.softnodes = SOFT_NODE_COUNT;
            limit.maxnodes = HARD_NODE_COUNT;
            limit.start();
            thread->nodes = 0;
            thread->bestMove = Move::NO_MOVE;
            
            int eval = Search::iterativeDeepening(board, *thread, limit, nullptr);
            TT.incAge();
            
            eval = std::min(std::max(-INFINITE, eval), INFINITE);
            eval = board.sideToMove() == Color::WHITE ? eval : -eval;
            Move m = thread->bestMove;
            moveScoreBuffer.emplace_back(packMove(m), (int16_t)eval);
            board.makeMove(thread->bestMove);
            end = board.isGameOver();
            poses++;
            cached++;
        }
        double wdl;
        if (!board.inCheck()) {
            marlin.wdl = 1;
        } else if (board.sideToMove() == Color::WHITE)
            marlin.wdl = 0;
        else
            marlin.wdl = 2;

        gameBuffer.emplace_back(marlin, moveScoreBuffer);

        if (G % 50 == 0) {
            std::cout << "Thread: " << ti << " Total Games: " << G << " Positions: " << poses << " Estimated speed "
                      << cached * 1000.0 / (double)timer.elapsed() << " pos/s" << std::endl;
            timer.start();
            cached = 0;
        }
        if (G % GAMES_BUFFER == 0) {
            std::cout << "Thread: " << ti << " writing " << GAMES_BUFFER << " games" << std::endl;
            for (ViriEntry& game : gameBuffer) {
                writeViriformat(outFile, game);
            }
            gameBuffer.clear();
        }
    }
}

void startDatagen(size_t tcount, bool isDFRC) {
    std::vector<std::thread> threads;

    for (size_t i = 0; i < tcount - 1; i++) {
        threads.emplace_back(runThread, i, isDFRC);
    }
    runThread(tcount - 1, isDFRC);
}

double rfpStats(Searcher& searcher) {
    std::ifstream file("data/lichess.book");
    std::string line;

    if (!file) {
        std::cerr << "Error: Could not open file\n";
        return 0 ;
    }
    int poses = 0;
    int maxPoses = 1000;

    TimeLimit timer = TimeLimit();
    searcher.printInfo = false;
    searcher.waitForSearchFinished();
    searcher.reset();


    while (std::getline(file, line) && poses < maxPoses) {
        size_t pos = line.find(" [");
        if (pos != std::string::npos) {
            line = line.substr(0, pos);
        }

        Board board(line);
        Search::Limit limit = Search::Limit();
        limit.depth = (int64_t)BENCH_DEPTH;
        limit.movetime = 0;
        limit.ctime = 0;
        limit.start();

        searcher.startSearching(board, limit);
        searcher.waitForSearchFinished();
        searcher.reset();

        poses++;

        std::cout << "Position " << poses << "/" << maxPoses << std::endl;
        //std::cout << "RFP Accuracy (Correctly triggered rfp / total rfps triggered): " << 100 * searcher.correctRfps / (double)searcher.totalRfps << "%" << " or " << searcher.correctRfps << "/" << searcher.totalRfps << std::endl;
        //std::cout << "RFP Impact (Correctly triggered rfp / total beta cuts): " << 100 * searcher.correctRfps / (double)searcher.totalBetaCuts << "%" << " or " << searcher.correctRfps << "/" << searcher.totalBetaCuts << std::endl;

        // std::cout << "Actual Positives: " << searcher.actualPositives << std::endl;
        // std::cout << "Actual Negatives: " << searcher.actualNegatives << std::endl;
        // std::cout << "Predicted Positives: " << searcher.predictedPositives << std::endl;
        // std::cout << "Predicted Negatives: " << searcher.predictedNegatives << std::endl;
        // std::cout << "\n";
        std::cout << "True Positives: " << searcher.tp << std::endl;
        std::cout << "True Negatives: " << searcher.tn << std::endl;
        std::cout << "False Positives: " << searcher.fp << std::endl;
        std::cout << "False Negatives: " << searcher.fn << std::endl;

        /*
        Standard
        RFP Accuracy (Correctly triggered rfp / total rfps triggered): 94.7041% or 41134388/43434661
        RFP Impact (Correctly triggered rfp / total beta cuts): 82.7217% or 41134388/49726225

        RFP Accuracy (Correctly triggered rfp / total rfps triggered): 94.7856% or 41067927/43327159
        RFP Impact (Correctly triggered rfp / total beta cuts): 83.1536% or 41067927/49388045

        Z-scores = -17, -57

        */
    }
    return 1;

}
