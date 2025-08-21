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
std::array<int, LMR_ONE_COUNT> LMR_ONE_PAIR = {66, 978, -835, 1797, -818, -155, 851};
std::array<int, LMR_TWO_COUNT> LMR_TWO_PAIR = {-84, -175, 88, 166, -53, 153, -234, 220, -24, -31,
                                                128, -21, -198, 38, -103, 9, -160, 83, 353, -206, 76};
std::array<int, LMR_THREE_COUNT> LMR_THREE_PAIR = {-181, -108, -143, -189, -219, 187, -180, 158, 10, -10,
                                                    228, -84, 123, 76, -35, -47, 142, -116, 22, 162,
                                                    -57, -145, 207, 22, -37, -286, 137, 233, -131, 39,
                                                    2, 169, 19, 87, -85};
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

int lmrInference(bool isQuiet, bool pv, bool improving, bool cutnode, bool ttpv, bool tthit, bool failhigh) {
    std::array<double, 32> H1 = {0};
    double output = LMR_OUTPUT_BIAS;
    for (int h = 0; h < 32; h++) {
        H1[h] += LMR_H1_BIAS[h];
        H1[h] += isQuiet * LMR_H1[h][0];
        H1[h] += pv * LMR_H1[h][1];
        H1[h] += improving * LMR_H1[h][2];
        H1[h] += cutnode * LMR_H1[h][3];
        H1[h] += ttpv * LMR_H1[h][4];
        H1[h] += tthit * LMR_H1[h][5];
        H1[h] += failhigh * LMR_H1[h][6];
        H1[h] = std::max(0.0, H1[h]) + 1e-2 * std::min(0.0, H1[h]);
    }
    for (int h = 0; h < 32; h++) {
        output += H1[h] * LMR_OUTPUT[h];
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