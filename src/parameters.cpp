#include "parameters.h"
#include "external/chess.hpp"
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstring>
#include <sstream>
#include <fstream>
#include <vector>

using namespace chess;

std::array<int, 6> MVV_VALUES = {MVV_PAWN_VALUE(), MVV_KNIGHT_VALUE(), MVV_BISHOP_VALUE(), MVV_ROOK_VALUE(), MVV_QUEEN_VALUE(), 0};

int lmrConvolution(std::array<bool, LMR_ONE_COUNT> features) {
    int output = 0;
    int twoIndex = 0;
    int threeIndex = 0;
    for (int i = 0; i < LMR_ONE_COUNT; i++) {
        output += LMR_ONE_PAIR[i] * features[i];

        for (int j = i + 1; j < LMR_ONE_COUNT; j++) {
            output += LMR_TWO_PAIR[twoIndex] * (features[i] && features[j]);

            for (int k = j + 1; k < LMR_ONE_COUNT; k++) {
                output += LMR_THREE_PAIR[threeIndex] * (features[i] && features[j] && features[k]);

                threeIndex++;
            }
            twoIndex++;
        }
    }
    return output;
}

void printWeatherFactoryConfig() {
    std::cout << "{\n";
    for (auto& param : tunables()) {
        std::cout << "  \"" << param.name << "\": {\n";
        std::cout << "    \"value\": " << param.defaultValue << ",\n";
        std::cout << "    \"min_value\": " << param.min << ",\n";
        std::cout << "    \"max_value\": " << param.max << ",\n";
        std::cout << "    \"step\": " << param.step << "\n";
        std::cout << "  }";
        if (&param != &tunables().back())
            std::cout << ",";
        std::cout << "\n";
    }
    for (int i = 0; i < LMR_ONE_PAIR.size(); i++) {
        std::cout << "  \"" << ("LMR_ONE_PAIR_" + std::to_string(i)) << "\": {\n";
        std::cout << "    \"value\": " << LMR_ONE_PAIR[i] << ",\n";
        std::cout << "    \"min_value\": " << (-2048) << ",\n";
        std::cout << "    \"max_value\": " << (2048) << ",\n";
        std::cout << "    \"step\": " << (200) << "\n";
        std::cout << "  }";
        if (i != LMR_ONE_PAIR.size() - 1)
            std::cout << ",";
        std::cout << "\n";
    }
    for (int i = 0; i < LMR_TWO_PAIR.size(); i++) {
        std::cout << "  \"" << ("LMR_TWO_PAIR_" + std::to_string(i)) << "\": {\n";
        std::cout << "    \"value\": " << LMR_TWO_PAIR[i] << ",\n";
        std::cout << "    \"min_value\": " << (-2048) << ",\n";
        std::cout << "    \"max_value\": " << (2048) << ",\n";
        std::cout << "    \"step\": " << (200) << "\n";
        std::cout << "  }";
        if (i != LMR_TWO_PAIR.size() - 1)
            std::cout << ",";
        std::cout << "\n";
    }
    for (int i = 0; i < LMR_THREE_PAIR.size(); i++) {
        std::cout << "  \"" << ("LMR_THREE_PAIR_" + std::to_string(i)) << "\": {\n";
        std::cout << "    \"value\": " << LMR_THREE_PAIR[i] << ",\n";
        std::cout << "    \"min_value\": " << (-2048) << ",\n";
        std::cout << "    \"max_value\": " << (2048) << ",\n";
        std::cout << "    \"step\": " << (200) << "\n";
        std::cout << "  }";
        if (i != LMR_THREE_PAIR.size() - 1)
            std::cout << ",";
        std::cout << "\n";
    }
    std::cout << "}" << std::endl;
}

void printOBConfig() {
    for (auto& param : tunables()) {
        std::cout << param.name << ", ";
        std::cout << "int, ";
        std::cout << param.defaultValue << ", ";
        std::cout << param.min << ", ";
        std::cout << param.max << ", ";
        std::cout << param.step << ", ";
        std::cout << "0.002" << std::endl;
    }
    for (int i = 0; i < LMR_ONE_PAIR.size(); i++) {
        std::cout << "LMR_ONE_PAIR_" + std::to_string(i) << ", int, " << LMR_ONE_PAIR[i];
        std::cout << ", -2048, 2048, 200, 0.002" << std::endl;
    }
    for (int i = 0; i < LMR_TWO_PAIR.size(); i++) {
        std::cout << "LMR_TWO_PAIR_" + std::to_string(i) << ", int, " << LMR_TWO_PAIR[i];
        std::cout << ", -2048, 2048, 200, 0.002" << std::endl;
    }
    for (int i = 0; i < LMR_THREE_PAIR.size(); i++) {
        std::cout << "LMR_THREE_PAIR_" + std::to_string(i) << ", int, " << LMR_THREE_PAIR[i];
        std::cout << ", -2048, 2048, 200, 0.002" << std::endl;
    }
}

void readSPSAOutput(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    std::array<int, LMR_ONE_COUNT> unus = {0};
    std::array<int, LMR_TWO_COUNT> duo = {0};
    std::array<int, LMR_THREE_COUNT> tres = {0};

    while (std::getline(file, line)) {
        std::string name = line.substr(0, line.find(","));
        int value = std::stoi(line.substr(line.find(",") + 2));
        if (name.find("[") == std::string::npos && name.find("PAIR") == std::string::npos) {
            // Find max min step etc
            for (auto& param : tunables()) {
                if (param.name == name)
                    std::cout << "TUNABLE_PARAM(" << name << ", " 
                                                    << value << ", " 
                                                    << param.min << ", " 
                                                    << param.max << ", " 
                                                    << param.step << ")" << std::endl;
            }
            
        }
        else if (name.find("[") != std::string::npos) {
            int index = std::stoi(line.substr(line.find("[") + 1, line.find("]")));
            std::string strip = name.substr(0, line.find("["));
            if (index == 0) {
                std::cout << "TUNABLE_TWOWAY(\n    " 
                            << strip << ",\n    "
                            << "std::array<int, TWO_WAY_WEIGHT_COUNT>{";
            }
            std::cout << value;
            if (index == TWO_WAY_WEIGHT_COUNT - 1) {
                std::cout << "}\n);" << std::endl;
            }
            else {
                std::cout << ", ";
            }
        }
        else {
            int index = std::stoi(line.substr(line.rfind("_") + 1, line.rfind(",")));
            if (line.find("ONE") != std::string::npos)
                unus[index] = value;
            else if (line.find("TWO") != std::string::npos)
                duo[index] = value;
            else
                tres[index] = value;
        }
    }
    std::cout << "inline std::array<int, LMR_ONE_COUNT> LMR_ONE_PAIR = {";
    for (int& v : unus)
        std::cout << v << ", ";
    std::cout << "};" << std::endl;

    std::cout << "inline std::array<int, LMR_TWO_COUNT> LMR_TWO_PAIR = {";
    for (int& v : duo)
        std::cout << v << ", ";
    std::cout << "};" << std::endl;
    std::cout << "inline std::array<int, LMR_THREE_COUNT> LMR_THREE_PAIR = {";
    for (int& v : tres)
        std::cout << v << ", ";
    std::cout << "};" << std::endl;
}