#pragma once

#include <chrono>

namespace feline {

class Timer {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start_;
public:
    Timer() : start_(clock::now()) {}

    void reset() { start_ = clock::now(); }

    double elapsed_ms() const {
        auto now = clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

    double elapsed_s() const { return elapsed_ms() / 1000.0; }
};

} // namespace feline
