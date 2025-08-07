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

// RFP

std::array<int, RFP_HL_N * RFP_INPUTS + RFP_HL_N> RFP_HL = {167, 171, 1068, 1416, -1791, -1257, 5282, -7679, 548, 8710, -7805, 403, 
                                                                -23, -28, -2696, 313};
std::array<int, RFP_HL_N + 1> RFP_OUTPUT = {1427, 1338, 2762, 6234, -2494};

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

int rfpInference(std::array<int, RFP_INPUTS> inputs) {
    std::array<int, RFP_HL_N> HL = {0};
    int output = RFP_OUTPUT[4];
    for (int i = 0; i < RFP_HL_N; i++) {
        HL[i] += RFP_HL[3 * i] * inputs[0] + RFP_HL[3 * i + 1] * inputs[1] + RFP_HL[3 * i + 2] * inputs[2];
        HL[i] += RFP_HL[12 + i];
        HL[i] = std::max(0, HL[i]) >> 10;
    }
    for (int i = 0; i < RFP_HL_N; i++) {
        output += HL[i] * RFP_OUTPUT[i];
    }
    return output >> 10;
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