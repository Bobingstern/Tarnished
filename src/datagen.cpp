#include "datagen.h"
#include "search.h"
#include "searcher.h"
#include "nnue.h"
#include "tt.h"
#include "timeman.h"
#include "eval.h"
#include <atomic>
#include <vector>
#include <random>
#include <thread>
#include <cstdlib>
#include <fstream>
#include <filesystem>


using namespace chess;





MarlinFormat::MarlinFormat(Board &board){
	const uint8_t stm = board.sideToMove() == Color::BLACK ? (1 << 7) : 0;
	occupancy = board.occ().getBits();
	epSquare = stm | (board.enpassantSq() == Square::NO_SQ ? 64 : (board.enpassantSq()^7).index());
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
		Board::CastlingRights cr = board.castlingRights();
		if (pt == PieceType::ROOK
			&& ((sq == Square::SQ_A1 && cr.has(Color::WHITE, Board::CastlingRights::Side::QUEEN_SIDE))
				|| (sq == Square::SQ_H1 && cr.has(Color::WHITE, Board::CastlingRights::Side::KING_SIDE))
				|| (sq == Square::SQ_A8 && cr.has(Color::BLACK, Board::CastlingRights::Side::QUEEN_SIDE))
				|| (sq == Square::SQ_H8 && cr.has(Color::BLACK, Board::CastlingRights::Side::KING_SIDE))))
			pt = PieceType::NONE; // This is bad but everything will have a piecetype cuz occupancy only, therefore NONE is unique on this condition only

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
	GameEntry(std::string f, std::vector<Move> m, std::vector<int> s, double w){
		fen = f;
		moves = m;
		scores = s;
		wdl = w;
	}
};


void makeRandomMove(Board &board){
	Movelist moves;
	movegen::legalmoves(moves, board);

	std::random_device rd;
	std::mt19937_64 engine(rd());
	std::uniform_int_distribution<int> dist(0, moves.size() - 1);

	board.makeMove(moves[dist(engine)]);
}

void writeBuffer(std::ofstream &outFile, GameEntry &game){
	Board test = Board(game.fen);
	for (int i=0;i<game.moves.size();i++){
		test.makeMove(game.moves[i]);
		outFile << test.getFen() << " | " << game.scores[i] << " | " << game.wdl << std::endl;
	}
	outFile.flush();
}

void writeViriformat(std::ofstream &outFile, ViriEntry &game){
	int32_t nullbytes = 0;
	outFile.write(reinterpret_cast<const char*>(&game.header), sizeof(game.header));
	outFile.write(reinterpret_cast<const char*>(game.scores.data()), sizeof(ScoredMove) * game.scores.size());
	outFile.write(reinterpret_cast<const char*>(&nullbytes), 4);
	outFile.flush();
}


uint16_t packMove(Move m){
	uint16_t move = 0;
	uint16_t from = (m.from()).index();
	uint16_t to = (m.to()).index();
	uint16_t flag = 0;
	// God save my soul
	if (m.typeOf() == Move::CASTLING){
		flag = 0b10'00'000000'000000;
		if (to == 6)
			to = 7;
		else if (to == 1)
			to = 0;
		else if (to == 62)
			to = 63;
		else if (to == 57)
			to = 56;
	}
	else if (m.typeOf() == Move::ENPASSANT){
		flag = 0b01'00'000000'000000;
	}
	else if (m.typeOf() == Move::PROMOTION){
		flag = 0b11'00'000000'000000 | ( ((uint16_t)m.promotionType()-1) << 12);
	}
	move |= (uint16_t)(from);
	move |= (uint16_t)(to) << 6;
	move |= flag;
	return move;
}

void runThread(int ti) {
	std::string filePath = "data/nnue_thread" + std::to_string(ti) + ".vf";

	if (!std::filesystem::is_directory("data/"))
		std::filesystem::create_directory("data/");
	std::ofstream outFile(filePath, std::ios::app | std::ios::binary);

	int64_t cached = 0;
	std::vector<ScoredMove> moveScoreBuffer;
	std::vector<ViriEntry> gameBuffer;

	TTable TT;
	std::unique_ptr<Search::ThreadInfo> thread = std::make_unique<Search::ThreadInfo>(ThreadType::SECONDARY, TT, nullptr);
	thread->stopped = false;
	moveScoreBuffer.reserve(256);
	gameBuffer.clear();
	// Play a game
	int64_t poses = 0;
	TimeLimit timer;
	timer.start();
	for (int G=1;G<1'000'000;G++){
		Board board;
		thread->reset();
		TT.clear();
		moveScoreBuffer.clear();

		for (size_t i=0;i<DATAGEN_RANDOM_MOVES;i++){
			makeRandomMove(board);
			if (board.isGameOver().second != GameResult::NONE)
				break;
		}
		MarlinFormat marlin = MarlinFormat(board);
		std::string startingFen = board.getFen();
		std::pair<GameResultReason, GameResult> end = board.isGameOver();

		while (end.second == GameResult::NONE){
			Search::Limit limit = Search::Limit();
			limit.softnodes = SOFT_NODE_COUNT;
			limit.maxnodes = HARD_NODE_COUNT;
			limit.start();
			thread->nodes = 0;
			thread->bestMove = Move::NO_MOVE;
			int eval = Search::iterativeDeepening(std::ref(board), *thread, limit, nullptr);
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
		if (!board.inCheck()){
			marlin.wdl = 1;
		}
		else if (board.sideToMove() == Color::WHITE)
			marlin.wdl = 0;
		else 
			marlin.wdl = 2;

		//ViriEntry game = ViriEntry(marlin, moveScoreBuffer);
		//writeViriformat(outFile, game);
		gameBuffer.emplace_back(marlin, moveScoreBuffer);

		if (G % 50 == 0){
			std::cout << "Thread: " << ti << " Total Games: " << G << " Positions: " << poses << " Estimated speed " << cached*1000.0/(double)timer.elapsed() << " pos/s" << std::endl;
			timer.start();
			cached = 0;
		}
		if (G % GAMES_BUFFER == 0){
			std::cout << "Thread: " << ti << " writing " << GAMES_BUFFER << " games" << std::endl;
			for (ViriEntry &game : gameBuffer){
				writeViriformat(outFile, game);
			}
			gameBuffer.clear();
		}
		//std::cout << "Finished " << G << " games" << std::endl;
	}
}	

void startDatagen(size_t tcount){
	std::vector<std::thread> threads;

	for (size_t i=0;i<tcount-1;i++){
		threads.emplace_back(runThread, i);
	}
	runThread(tcount-1);
}

#ifdef STORE_LMR_DATA
void lmrDatagen() {
	
	
	TTable TT;
	std::unique_ptr<Search::ThreadInfo> thread = std::make_unique<Search::ThreadInfo>(ThreadType::SECONDARY, TT, nullptr);
	thread->stopped = false;
	std::vector<std::string> fens;
	std::ifstream file("./data/fens.fens");
	std::ofstream fileOut("./data/lmrdata.csv", std::ios::app); // Open in append mode
	std::string line;
    std::mt19937 g;
    g.seed(0);

	if (file.is_open()) {
		while (std::getline(file, line)) {
			if (line.length() > 2)
				fens.push_back(line);
		}
	}
	file.close();

	std::shuffle(fens.begin(), fens.end(), g);
	fens.resize(1000);

    double avg = 0;
    int total = 0;
    int c = 0;
    for (auto fen : fens) {
    	Board board;
		board.setFen(fen);
		thread->reset();
		TT.clear();

		if (board.isGameOver().second != GameResult::NONE)
			continue;
		
		Search::Limit limit = Search::Limit();
		limit.softnodes = 50000;
		limit.maxnodes = HARD_NODE_COUNT;
		limit.start();
		thread->nodes = 0;
		thread->bestMove = Move::NO_MOVE;
		int eval = Search::iterativeDeepening(std::ref(board), *thread, limit, nullptr);
		//std::shuffle(thread->lmrInfo.begin(), thread->lmrInfo.end(), g);
		//thread->lmrInfo.resize(1000);

		// Attempt to equalize distribution of nonzeros with zeros
		const int zeros = std::count_if(thread->lmrInfo.begin(), thread->lmrInfo.end(), [](Search::LMRInfo a) {return a.optimalReduction == 0;});
		std::vector<int> nonzeroIndex;
		std::vector<Search::LMRInfo> data;
		for (int i=0;i<thread->lmrInfo.size();i++) {
			if (thread->lmrInfo[i].optimalReduction == 0)
				data.push_back(thread->lmrInfo[i]);
			else
				nonzeroIndex.push_back(i);
		}
		std::shuffle(nonzeroIndex.begin(), nonzeroIndex.end(), g);
		for (int i=0;i<zeros;i++) {
			data.push_back(thread->lmrInfo[nonzeroIndex[i]]);
		}

		for (auto &entry : data) {
			// Save data
			fileOut << std::fixed << std::setprecision(6)
	             << entry.depth / 256.0 << ','
	             << entry.moveCount / 256.0 << ','
	             << entry.optimalReduction / 256.0 << "\n";
			//std::cout << "Reduction " << entry.reduction << " Optimal " << entry.optimalReduction << std::endl;
		}
		total += data.size();
		c++;

		std::cout << "Fens " << c  << " Total " << total << std::endl;

	}

}
#endif