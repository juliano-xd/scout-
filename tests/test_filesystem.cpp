#include <gtest/gtest.h>
#include "utils/filesystem.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

class FsUtilsTest : public ::testing::Test {
protected:
    fs::path test_dir;

    void SetUp() override {
        // Criar diretório temporário para testes
        test_dir = fs::temp_directory_path() / "scout_test_dir";
        fs::create_directories(test_dir / "subfolder");
        
        // Criar um arquivo de teste
        std::ofstream(test_dir / "target_file.txt") << "dummy content";
        std::ofstream(test_dir / "subfolder" / "deep_file.smali") << "dummy content";
    }

    void TearDown() override {
        // Limpar o diretório de testes
        fs::remove_all(test_dir);
    }
};

TEST_F(FsUtilsTest, ThrowsWhenDirectoryDoesNotExist) {
    fs::path invalid_dir = test_dir / "does_not_exist";
    EXPECT_THROW(utils::buscar_arquivo_recursivo(invalid_dir, "file.txt"), std::invalid_argument);
}

TEST_F(FsUtilsTest, FindsFileInRootDirectory) {
    auto result = utils::buscar_arquivo_recursivo(test_dir, "target_file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->filename(), "target_file.txt");
}

TEST_F(FsUtilsTest, FindsFileInSubdirectory) {
    auto result = utils::buscar_arquivo_recursivo(test_dir, "deep_file.smali");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->filename(), "deep_file.smali");
}

TEST_F(FsUtilsTest, ReturnsNulloptWhenFileNotFound) {
    auto result = utils::buscar_arquivo_recursivo(test_dir, "non_existent.txt");
    EXPECT_FALSE(result.has_value());
}

TEST_F(FsUtilsTest, HandlesEmptyFilename) {
    auto result = utils::buscar_arquivo_recursivo(test_dir, "");
    EXPECT_FALSE(result.has_value());
}

TEST_F(FsUtilsTest, HandlesSpecialCharactersInFilename) {
    std::string special_name = "file with space & symbols #!%.txt";
    std::ofstream(test_dir / special_name) << "special";
    auto result = utils::buscar_arquivo_recursivo(test_dir, special_name);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->filename(), special_name);
}

TEST_F(FsUtilsTest, HandlesDeeplyNestedFile) {
    fs::path deep_path = test_dir;
    for(int i=0; i<10; ++i) {
        deep_path /= "dir" + std::to_string(i);
    }
    fs::create_directories(deep_path);
    std::ofstream(deep_path / "deepest.txt") << "bottom";

    auto result = utils::buscar_arquivo_recursivo(test_dir, "deepest.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->filename(), "deepest.txt");
}

TEST_F(FsUtilsTest, ThrowsWhenPathIsFileInsteadOfDirectory) {
    fs::path file_path = test_dir / "target_file.txt";
    EXPECT_THROW(utils::buscar_arquivo_recursivo(file_path, "any.txt"), std::invalid_argument);
}

TEST_F(FsUtilsTest, HandlesLargeNumberOfFiles) {
    for(int i=0; i<100; ++i) {
        std::ofstream(test_dir / ("file_" + std::to_string(i) + ".txt")) << i;
    }
    auto result = utils::buscar_arquivo_recursivo(test_dir, "file_99.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->filename(), "file_99.txt");
}
