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
std::array<int, LMR_ONE_COUNT> LMR_ONE_PAIR = {128, 1019, -880, 1835, -945, 211, 477, -522};
std::array<int, LMR_TWO_COUNT> LMR_TWO_PAIR = {
                                235, -227, 433, -177, -18, -1, 16, -529,
                                483, -47, 267, -159, -188, 155, -39, 234,
                                67, 123, 173, 125, 589, -628, 672, -32,
                                58, -193, -69, 60
                            };
std::array<int, LMR_THREE_COUNT> LMR_THREE_PAIR = {
                                -80, -556, 90, -69, -325, -75, 385, -399,
                                -117, -208, -271, -164, 516, 70, 115, 47,
                                -224, 244, -423, -152, -245, -120, 522, -99,
                                271, 123, 106, -194, -720, -12, 121, -286,
                                -160, 152, -176, 297, -314, -237, -220, -68,
                                -638, 127, 393, 263, 120, 229, 193, -151,
                                -531, -55, -69, -44, 365, -267, 254, -34
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