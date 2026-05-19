#include "../../include/utils/progress_reporter.hpp"

#include <print>
#include <chrono>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace scout {

ProgressReporter::ProgressReporter(
    std::string_view engine,
    std::string_view query,
    FILE* output
) : output_(output)
  , engine_(engine)
  , query_(query)
{
#ifdef _WIN32
    is_tty_ = _isatty(_fileno(output_));
#else
    is_tty_ = isatty(fileno(output_));
#endif
}

ProgressReporter::~ProgressReporter() {
    if (!finished_ && started_) {
        fail("engine did not finish normally");
    }
}

void ProgressReporter::start() {
    started_ = true;
    start_time_ = std::chrono::steady_clock::now();

    auto n = sexpr::form("scout:progress");
    n.kv("engine", sexpr::string(engine_));
    n.kv("status", sexpr::string("running"));
    n.kv("timestamp", sexpr::string(sexpr::current_timestamp()));
    if (!query_.empty()) n.kv("query", sexpr::string(query_));

    write_line(n.to_string(false), false);
}

void ProgressReporter::finish(std::size_t result_count) {
    finished_ = true;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_).count();

    auto n = sexpr::form("scout:progress");
    n.kv("engine", sexpr::string(engine_));
    n.kv("status", sexpr::string("done"));
    n.kv("results", sexpr::integer(static_cast<long long>(result_count)));
    n.kv("elapsed_ms", sexpr::integer(elapsed));

    write_line(n.to_string(false), true);
}

void ProgressReporter::fail(std::string_view reason) {
    finished_ = true;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_).count();

    auto n = sexpr::form("scout:progress");
    n.kv("engine", sexpr::string(engine_));
    n.kv("status", sexpr::string("failed"));
    n.kv("elapsed_ms", sexpr::integer(elapsed));
    if (!reason.empty()) n.kv("reason", sexpr::string(reason));

    write_line(n.to_string(false), true);
}

void ProgressReporter::write_line(const std::string& line, bool finalize) {
    if (is_tty_) {
        std::print(output_, "\r{}\033[K", line);
        if (finalize) std::println(output_, "");
        std::fflush(output_);
    } else {
        std::println(output_, "{}", line);
    }
}

} // namespace scout
