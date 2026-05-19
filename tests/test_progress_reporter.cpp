#include <gtest/gtest.h>

#include "utils/progress_reporter.hpp"

#include <cstdio>
#include <cstring>
#include <string>

class ProgressReporterTest : public ::testing::Test {
protected:
    static constexpr std::size_t BUF_SIZE = 8192;
    char* mem_buffer_;
    FILE* mem_file_;

    void SetUp() override {
        mem_buffer_ = new char[BUF_SIZE];
        std::memset(mem_buffer_, 0, BUF_SIZE);
        mem_file_ = fmemopen(mem_buffer_, BUF_SIZE, "w+");
    }

    void TearDown() override {
        if (mem_file_) {
            fclose(mem_file_);
            mem_file_ = nullptr;
        }
        delete[] mem_buffer_;
    }

    std::string read_output() {
        std::fflush(mem_file_);
        return std::string(mem_buffer_);
    }
};

TEST_F(ProgressReporterTest, StartEmitsRunning) {
    {
        scout::ProgressReporter pr("test_engine", "test_query", mem_file_);
        pr.start();
    }

    auto output = read_output();
    EXPECT_NE(output.find(":engine \"test_engine\""), std::string::npos);
    EXPECT_NE(output.find(":status \"running\""), std::string::npos);
    EXPECT_NE(output.find(":timestamp"), std::string::npos);
}

TEST_F(ProgressReporterTest, FinishEmitsDoneWithResults) {
    {
        scout::ProgressReporter pr("xref", "Lcom/Test;", mem_file_);
        pr.start();
        pr.finish(42);
    }

    auto output = read_output();
    EXPECT_NE(output.find(":engine \"xref\""), std::string::npos);
    EXPECT_NE(output.find(":status \"done\""), std::string::npos);
    EXPECT_NE(output.find(":results 42"), std::string::npos);
    EXPECT_NE(output.find(":elapsed_ms"), std::string::npos);
}

TEST_F(ProgressReporterTest, DestructorEmitsFailedWhenNotFinished) {
    {
        scout::ProgressReporter pr("crash_engine", "", mem_file_);
        pr.start();
    }

    auto output = read_output();
    EXPECT_NE(output.find(":status \"failed\""), std::string::npos);
}

TEST_F(ProgressReporterTest, NoOutputToStdout) {
    char stdout_buf[256] = {0};
    FILE* stdout_capture = fmemopen(stdout_buf, sizeof(stdout_buf), "w+");
    FILE* old_stdout = stdout;
    stdout = stdout_capture;

    {
        scout::ProgressReporter pr("test", "", stderr);
        pr.start();
        pr.finish(0);
    }

    stdout = old_stdout;
    fclose(stdout_capture);

    EXPECT_EQ(std::strlen(stdout_buf), 0UL);
}

TEST_F(ProgressReporterTest, ValidSExpression) {
    scout::ProgressReporter pr("test", "q", mem_file_);
    pr.start();
    pr.finish(10);

    auto output = read_output();
    ASSERT_FALSE(output.empty());
    EXPECT_EQ(output.front(), '(');
    EXPECT_NE(output.find(')'), std::string::npos);
}

TEST_F(ProgressReporterTest, SequentialEngines) {
    {
        scout::ProgressReporter pr1("engine_a", "q1", mem_file_);
        pr1.start();
        pr1.finish(5);
    }
    {
        scout::ProgressReporter pr2("engine_b", "q2", mem_file_);
        pr2.start();
        pr2.finish(10);
    }

    auto output = read_output();
    EXPECT_NE(output.find("engine_a"), std::string::npos);
    EXPECT_NE(output.find("engine_b"), std::string::npos);
    EXPECT_NE(output.find(":results 5"), std::string::npos);
    EXPECT_NE(output.find(":results 10"), std::string::npos);
}

TEST_F(ProgressReporterTest, BothRunningAndDoneWhenNonTTY) {
    scout::ProgressReporter pr("test", "", mem_file_);
    pr.start();
    pr.finish(7);

    auto output = read_output();
    EXPECT_NE(output.find(":status \"running\""), std::string::npos);
    EXPECT_NE(output.find(":status \"done\""), std::string::npos);
}
