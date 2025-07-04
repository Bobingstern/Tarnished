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
std::array<int, LMR_NUM_ONE_PAIR> LMR_ONE_PAIR = {425, 875, -1729, 1752, -656, -556, 339, 176, -380, -1438};
std::array<int, LMR_NUM_TWO_PAIR> LMR_TWO_PAIR = {
    362, 679, -58,  465,   -1080, 143,   -385, 650,  289,   -1219, 1230,  -526, -321,  -199, -510,
    442, 678, 195,  -872,  196,   1457,  1699, -388, -1257, -1842, 890,   -3,   -1128, -527, 713,
    910, 648, 1103, -1077, 478,   -1234, -190, 1350, -735,  -691,  -1132, -792, 559,   -226, 1148};
std::array<int, LMR_NUM_THREE_PAIR> LMR_THREE_PAIR = {
    970,  1175,  843,  -837,  -715,  309,  985,   -290, 85,    -1170, -390, -803, -1936, -1420, -1423, 444,  602,  583,
    853,  -1094, 93,   -887,  -43,   307,  1274,  -335, 795,   -902,  -453, -145, -371,  97,    -1005, 1085, 617,  -15,
    -135, -168,  1063, -1078, -1651, -795, -268,  711,  592,   -37,   501,  952,  157,   1572,  -834,  991,  584,  1442,
    -249, -1707, 306,  70,    260,   890,  -1057, 1044, -665,  -475,  1830, 162,  609,   288,   67,    -535, 129,  1608,
    20,   59,    1165, 146,   303,   1834, -816,  405,  -2048, 194,   2002, 1136, -316,  4,     -1366, 723,  404,  -762,
    -926, -474,  1872, -1276, -1351, 1500, -1047, 583,  173,   1843,  -17,  -299, -935,  7,     -532,  1265, -462, 248,
    72,   -678,  -570, 175,   -540,  1717, -627,  -697, 1390,  -500,  1515, -362};
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

int lmrConvolution(std::array<bool, LMR_NUM_ONE_PAIR> features) {
    int output = 0;
    int twoIndex = 0;
    int threeIndex = 0;
    for (int i = 0; i < LMR_NUM_ONE_PAIR; i++) {
        output += LMR_ONE_PAIR[i] * features[i];

        for (int j = i + 1; j < LMR_NUM_ONE_PAIR; j++) {
            output += LMR_TWO_PAIR[twoIndex] * (features[i] && features[j]);

            for (int k = j + 1; k < LMR_NUM_ONE_PAIR; k++) {
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

void printLMRConfig() {
    for (auto& param : tunables()) {
        if (param.name.substr(0, 3) == "LMR") {
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