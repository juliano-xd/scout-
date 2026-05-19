#pragma once

#include "sexpr.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>

namespace scout {

class ProgressReporter {
public:
    explicit ProgressReporter(
        std::string_view engine,
        std::string_view query = "",
        FILE* output = stderr
    );

    ~ProgressReporter();

    void start();
    void finish(std::size_t result_count);
    void fail(std::string_view reason = "");

private:
    void write_line(const std::string& line, bool finalize);

    FILE* output_;
    std::string engine_;
    std::string query_;
    std::chrono::steady_clock::time_point start_time_;
    bool is_tty_ = false;
    bool started_ = false;
    bool finished_ = false;
};

} // namespace scout
