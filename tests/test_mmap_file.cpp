#include <gtest/gtest.h>
#include "utils/mmap_file.hpp"
#include <filesystem>
#include <fstream>
#include <string_view>

namespace fs = std::filesystem;

class MappedFileTest : public ::testing::Test {
protected:
    fs::path test_file;
    const std::string content = "Hello, Mmap!";

    void SetUp() override {
        test_file = fs::temp_directory_path() / "test_mmap.txt";
        std::ofstream ofs(test_file);
        ofs << content;
    }

    void TearDown() override {
        if (fs::exists(test_file)) {
            fs::remove(test_file);
        }
    }
};

TEST_F(MappedFileTest, OpensExistingFile) {
    utils::MappedFile mf(test_file);
    EXPECT_TRUE(mf.is_open());
    EXPECT_EQ(mf.size(), content.size());
    EXPECT_EQ(mf.view(), content);
}

TEST_F(MappedFileTest, FailsOnNonExistentFile) {
    utils::MappedFile mf("non_existent_file.txt");
    EXPECT_FALSE(mf.is_open());
    EXPECT_EQ(mf.size(), 0);
}

TEST_F(MappedFileTest, HandlesEmptyFile) {
    fs::path empty_file = fs::temp_directory_path() / "empty.txt";
    std::ofstream ofs(empty_file);
    ofs.close();

    utils::MappedFile mf(empty_file);
    // Based on implementation, size 0 returns early before mmap, so is_open() is false.
    EXPECT_FALSE(mf.is_open());
    EXPECT_EQ(mf.size(), 0);

    fs::remove(empty_file);
}

TEST_F(MappedFileTest, MoveConstructor) {
    utils::MappedFile mf1(test_file);
    ASSERT_TRUE(mf1.is_open());
    
    utils::MappedFile mf2(std::move(mf1));
    EXPECT_TRUE(mf2.is_open());
    EXPECT_EQ(mf2.view(), content);
    EXPECT_FALSE(mf1.is_open());
}

TEST_F(MappedFileTest, MoveAssignment) {
    utils::MappedFile mf1(test_file);
    utils::MappedFile mf2("non_existent.txt");
    
    mf2 = std::move(mf1);
    EXPECT_TRUE(mf2.is_open());
    EXPECT_EQ(mf2.view(), content);
    EXPECT_FALSE(mf1.is_open());
}

TEST_F(MappedFileTest, LargeFile) {
    fs::path large_file = fs::temp_directory_path() / "large_mmap.bin";
    std::ofstream ofs(large_file, std::ios::binary);
    size_t large_size = 1024 * 1024 * 2; // 2MB
    std::vector<char> large_content(large_size, 'A');
    ofs.write(large_content.data(), large_size);
    ofs.close();

    utils::MappedFile mf(large_file);
    EXPECT_TRUE(mf.is_open());
    EXPECT_EQ(mf.size(), large_size);
    EXPECT_EQ(mf.view().size(), large_size);
    EXPECT_EQ(mf.view()[0], 'A');
    EXPECT_EQ(mf.view()[large_size - 1], 'A');

    fs::remove(large_file);
}
