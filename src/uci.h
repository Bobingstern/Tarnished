#pragma once

#include "search.h"
#include "util.h"
#include "external/fmt/color.h"
#include "external/fmt/format.h"
#include <iostream>
#include <string.h>

using namespace chess;

// Yoinked from Weiss
// https://github.com/TerjeKir/weiss/blob/v1.0/src/uci.h
#define INPUT_SIZE 8192
enum InputCommands {
    // UCI
    GO = 11,
    UCI = 127,
    STOP = 28,
    QUIT = 29,
    ISREADY = 113,
    POSITION = 17,
    SETOPTION = 96,
    UCINEWGAME = 6,
    // Non-UCI
    BENCH = 99,
    EVAL = 26,
    PRINT = 112,
    DATAGEN = 124,
    WAIT = 15,
    CONFIG = 13
};
static bool GetInput(char* str) {
    memset(str, 0, INPUT_SIZE);
    if (fgets(str, INPUT_SIZE, stdin) == NULL)
        return false;
    str[strcspn(str, "\r\n")] = '\0';
    return true;
}
static bool BeginsWith(const char* str, const char* token) {
    return strstr(str, token) == str;
}
// Tests whether the name in the setoption string matches
static bool OptionName(const char* str, const char* name) {
    return BeginsWith(strstr(str, "name") + 5, name);
}
// Returns the (string) value of a setoption string
static char* OptionValue(const char* str) {
    const char* valuePtr = strstr(str, "value");
    return (valuePtr != nullptr && strlen(valuePtr) > 6) ? (char*)(valuePtr + 6) : nullptr;
}
// Sets a limit to the corresponding value in line, if any
static void SetLimit(const char* str, const char* token, int64_t* limit) {
    const char* ptr = strstr(str, token);
    if (ptr != nullptr) {
        *limit = std::stoll(ptr + strlen(token));
    }
}

static void bench(Searcher* searcher);

// ------------------------- ASCI ART -------------------------------

static void tarnishedAscii() {
    std::cout << COLORS::GREY << R"(
                         :=*#*+:                      
                     +#***********#                   
                  +#****************#                 
               .#*#******************@.               
           .:=#*%#********************%%:             
               :*************%***#****@*%             
              :%**************%**@*#*%**%             
        +:   .%*********#*****#*#@*#****%             
       ##   :@************#@*#*#%##**%*@:             
      :**#=##*****#**#=.-:%*@%#%#*##**%               
      :*******%*%***%-   #***@@%##*#@.                
       ##****%#**%*%.    =%======%+                   
        .=%%%%*-#=.     .%+=----=+%=                  
                     -@#--*+--::=*--#%-               
                   =#=--=*-#=-:-#-*---=%=             
                  #*-----++-%--%=*=-----**            
                  %:------#=%--%=*-------%            
                  %-------=+#=-**=------=#            
                 :%@%-=*--=+*==+*=--#=-#@@:           
                .%@=-=**+*%%#++*%%#+*+--=@@           
                 %%#+=----------------=+#%@           
                 +*%+%%@@%*======*%@@%%=%+#           
                 :%@==%#*%%%%%%%%%@##%==@#-           
                  =@+*%#==+++==+++==#%+=@#            
                   +=****+*#+--+#*+****=+             
                   =**@*%=%*%-:%*%=%*%**=             
                 .%@#*@*%=@+%-:%*@=%*@+%@#            
               .%+-+@=@*%*@+%-:%*@+%*@=@+:*%.         
            =%*--==*#@#=@*##%--%*#*@=#%%+=---#%=      
            #=#==+#+==+%#==%%=-%#+=%%+==+#+==*-#      
            +*+%#=------=*%%@==@%%*=------+*%+*=      
             =#=#+----------=*+=----------+#=#-       
               #+=%+---------+=---------*%=+#         
                 **=*%------+#++-----=%+=#*           
                   :#*=*%=:::+=---+%*=##:             
                      :*#+*#+***#*+#*:                
                         :##=**=##:                   
                            =%%-                      
 _______       _____  _   _ _____  _____ _    _ ______ _____  
|__   __|/\   |  __ \| \ | |_   _|/ ____| |  | |  ____|  __ \ 
   | |  /  \  | |__) |  \| | | | | (___ | |__| | |__  | |  | |
   | | / /\ \ |  _  /| . ` | | |  \___ \|  __  |  __| | |  | |
   | |/ ____ \| | \ \| |\  |_| |_ ____) | |  | | |____| |__| |
   |_/_/    \_\_|  \_\_| \_|_____|_____/|_|  |_|______|_____/                                    
    )" << "\n" << std::endl;
}

// ---------------------- Pretty Stuff

static void printBoard(Board& board) {
    std::cout << COLORS::WHITE
            << "\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510\n";

    int line = 1;
    for (int rank = 0; rank < 8; rank++) {
        std::cout << COLORS::WHITE << "\u2502 ";
        for (int file = 0; file < 8; file++) {
            Square sq = Square(Rank(rank), File(file));
            Piece p = board.at(sq);
            auto c = p.color() == Color::WHITE ? COLORS::YELLOW : COLORS::CYAN;
            if (p == Piece::NONE)
                c = COLORS::WHITE;
            std::cout << c << std::string(p) << COLORS::RESET << " ";
        }
        std::cout << COLORS::WHITE << "\u2502 \n";
    }
    std::cout << COLORS::WHITE
            << "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518\n";

}

static void printBoard(Board& board, Move move) {
    std::cout << COLORS::WHITE
            << "\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510\n";

    int line = 1;
    Piece p;
    std::string_view c;
    auto doPieceColor = [&](Square sq) {
        p = board.at(sq);
        c = p.color() == Color::WHITE ? COLORS::YELLOW : COLORS::CYAN;
        if (p == Piece::NONE)
            c = COLORS::WHITE;
        if (sq == move.from())
            c = COLORS::GREY;
        if (sq == move.to()){
            c = COLORS::GREEN;
            p = board.at(move.from());
            if (move.typeOf() == Move::PROMOTION)
                p = Piece(move.promotionType(), board.sideToMove());
        }
    };
    for (int rank = 0; rank < 8; rank++) {
        std::cout << COLORS::WHITE << "\u2502 ";
        for (int file = 0; file < 8; file++) {
            Square sq = Square(Rank(7 - rank), File(file));
            doPieceColor(sq);
            std::cout << c << std::string(p) << COLORS::RESET << " ";
        }
        std::cout << COLORS::WHITE << "\u2502 " << (rank + 1) << "\n";
    }
    std::cout << COLORS::WHITE
            << "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518\n";
    std::cout << "  a b c d e f g h\n\n";
}

static void printBoard(Board& board, std::vector<Move> moves) {

    for (auto move : moves)
        std::cout << COLORS::WHITE << "\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510  ";
    std::cout << COLORS::WHITE << "\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510  ";
    std::cout << "\n";
    int line = 1;
    Piece p;
    std::string_view c;
    auto doPieceColor = [&](Board& testBoard, Move move, Square sq) {
        p = testBoard.at(sq);
        c = p.color() == Color::WHITE ? COLORS::YELLOW : COLORS::CYAN;
        if (p == Piece::NONE)
            c = COLORS::WHITE;
        if (move != Move::NO_MOVE){
            if (sq == move.from())
                c = COLORS::GREY;
            if (sq == move.to()){
                c = COLORS::GREEN;
                p = testBoard.at(move.from());
                if (move.typeOf() == Move::PROMOTION)
                    p = Piece(move.promotionType(), testBoard.sideToMove());
            }
        }
    };
    for (int rank = 0; rank < 8; rank++) {
        Board testBoard = board;

        std::cout << COLORS::WHITE << "\u2502 ";
        for (int file = 0; file < 8; file++) {
            Square sq = Square(Rank(7 - rank), File(file));
            doPieceColor(board, Move::NO_MOVE, sq);
            std::cout << c << std::string(p) << COLORS::RESET << " ";
        }
        std::cout << COLORS::WHITE << "\u2502 " << (rank + 1);

        for (Move move : moves) {
            std::cout << COLORS::WHITE << "\u2502 ";
            for (int file = 0; file < 8; file++) {
                Square sq = Square(Rank(7 - rank), File(file));
                doPieceColor(testBoard, move, sq);
                std::cout << c << std::string(p) << COLORS::RESET << " ";
            }
            std::cout << COLORS::WHITE << "\u2502 " << (rank + 1);
            testBoard.makeMove(move);
        }
        std::cout << "\n";
    }
    for (auto move : moves) {
        std::cout << COLORS::WHITE
                << "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518  ";
    }
    std::cout << COLORS::WHITE
                << "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518  ";
    std::cout << "\n";
    for (auto move : moves) {
        std::cout << "  a b c d e f g h    ";
    }
    std::cout << "  a b c d e f g h    ";
    std::cout << "\n\n";
    
}


extern bool PRETTY_PRINT;