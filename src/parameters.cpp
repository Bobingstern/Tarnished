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
std::array<int, LMR_NUM_ONE_PAIR> LMR_ONE_PAIR = {6, 682, -1104, 1994, -928, -295, 0, 0, 0, 0};
std::array<int, LMR_NUM_TWO_PAIR> LMR_TWO_PAIR = {0};
std::array<int, LMR_NUM_THREE_PAIR> LMR_THREE_PAIR = {0};
// Code from Sirius
// https://github.com/mcthouacbb/Sirius/blob/b80a3d18461d97e94ba3102bc3fb422db66f4e7d/Sirius/src/search_params.cpp#L17C1-L29C2
std::list<TunableParam>& tunables() {
    static std::list<TunableParam> params;
    return params;
}

TunableParam& addTunableParam(std::string name, int value, int min, int max,
                              int step) {
    tunables().push_back({name, value, value, min, max, step});
    TunableParam& param = tunables().back();
    return param;
}

int lmrConvolution(std::array<bool, LMR_NUM_ONE_PAIR> features) {
    int output = 0;
    int twoIndex = 0;
    int threeIndex = 0;
    for (int i = 0; i < LMR_NUM_ONE_PAIR; i++) {
        output += LMR_ONE_PAIR[i] * features[i];

        for (int j = i + 1; j < LMR_NUM_ONE_PAIR; j++) {
            output += LMR_TWO_PAIR[twoIndex] * (features[i] && features[j]);

            for (int k = j + 1; k < LMR_NUM_ONE_PAIR; k++) {
                output += LMR_THREE_PAIR[threeIndex] *
                          (features[i] && features[j] && features[k]);

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

void printLMRConfig() {
    for (auto& param : tunables()) {
        if (param.name.substr(0, 3) == "LMR"){
            std::cout << param.name << ", ";
            std::cout << "int, ";
            std::cout << param.defaultValue << ", ";
            std::cout << param.min << ", ";
            std::cout << param.max << ", ";
            std::cout << param.step << ", ";
            std::cout << "0.002" << std::endl;
        }
    }
    for (int i = 0; i < LMR_ONE_PAIR.size(); i++) {
        std::cout << "LMR_ONE_PAIR_" + std::to_string(i) << ", int, " << LMR_ONE_PAIR[i];
        std::cout << ", -2048, 2048, 200, 0.02" << std::endl;
    }
    for (int i = 0; i < LMR_TWO_PAIR.size(); i++) {
        std::cout << "LMR_TWO_PAIR_" + std::to_string(i) << ", int, " << LMR_TWO_PAIR[i];
        std::cout << ", -2048, 2048, 200, 0.02" << std::endl;
    }
    for (int i = 0; i < LMR_THREE_PAIR.size(); i++) {
        std::cout << "LMR_THREE_PAIR_" + std::to_string(i) << ", int, " << LMR_THREE_PAIR[i];
        std::cout << ", -2048, 2048, 200, 0.02" << std::endl;
    }
}