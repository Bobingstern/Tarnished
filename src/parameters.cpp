#include "parameters.h"
#include "external/chess.hpp"
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstring>
#include <sstream>
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
    // for (auto& param : twoWayParams()) {
    //     std::cout << param->name << ", ";
    //     std::cout << "int, ";
    //     std::cout << param->defaultValue << ", ";
    //     std::cout << param->min << ", ";
    //     std::cout << param->max << ", ";
    //     std::cout << param->step << ", ";
    //     std::cout << "0.002" << std::endl;
    // }
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