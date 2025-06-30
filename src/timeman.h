#pragma once

#include <chrono>

class TimeLimit {
        std::chrono::high_resolution_clock::time_point startTime;

    public:
        TimeLimit() {};

        void start() {
            startTime = std::chrono::high_resolution_clock::now();
        }
        int elapsed() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() -
                                                                         startTime)
                .count();
        }
};