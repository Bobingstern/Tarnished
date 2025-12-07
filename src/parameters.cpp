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

// LMR
std::array<int, LMR_ONE_COUNT> LMR_ONE_PAIR = {137, 918, -848, 2047, -639, 231, 828, -769 };
std::array<int, LMR_TWO_COUNT> LMR_TWO_PAIR = {
                                -29, -176, 31, 186, -135, 120, -179, -341,
                                360, -250, -70, 187, 49, 46, -215, -36,
                                -240, 200, 255, 26, 112, -106, 371, -242,
                                -14, 189, 141, 115
                            };
std::array<int, LMR_THREE_COUNT> LMR_THREE_PAIR = {
                                -99, -46, -445, -6, -494, -7, 328, -533,
                                68, 53, -8, 21, 341, -10, -7, -118,
                                53, -6, -108, -102, 2, -164, 57, -103,
                                322, -66, -35, 41, -304, -2, 374, -248,
                                -148, -49, 87, -22, -209, -187, 70, 155,
                                -370, 93, 23, 72, 119, 137, 56, -42,
                                11, -5, -67, -143, -21, -21, 132, -40
                            };
// Code from Sirius
// https://github.com/mcthouacbb/Sirius/blob/b80a3d18461d97e94ba3102bc3fb422db66f4e7d/Sirius/src/search_params.cpp#L17C1-L29C2
std::list<TunableParam>& tunables() {
    static std::list<TunableParam> params;
    return params;
}

TunableParam& addTunableParam(std::string name, int value, int min, int max, int step) {
    tunables().push_back({name, value, value, min, max, step});
    TunableParam& param = tunables().back();
    return param;
}

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