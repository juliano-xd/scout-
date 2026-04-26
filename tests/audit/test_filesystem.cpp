#include <gtest/gtest.h>
#include "utils/filesystem.hpp"
#include <fstream>

using namespace utils;

TEST(FilesystemAudit, RecursiveSearch) {
    fs::path root = "fs_audit_root";
    fs::remove_all(root);
    fs::create_directories(root / "a/b/c");
    
    // 1. Criar arquivos
    {
        std::ofstream(root / "root.txt");
        std::ofstream(root / "a/b/c/deep.txt");
    }
    
    // Match raiz
    auto res1 = buscar_arquivo_recursivo(root, "root.txt");
    ASSERT_TRUE(res1.has_value());
    EXPECT_EQ(res1->filename(), "root.txt");
    
    // Match profundo
    auto res2 = buscar_arquivo_recursivo(root, "deep.txt");
    ASSERT_TRUE(res2.has_value());
    EXPECT_EQ(res2->filename(), "deep.txt");
    
    // Não encontrado
    auto res3 = buscar_arquivo_recursivo(root, "missing.txt");
    EXPECT_FALSE(res3.has_value());
    
    // Erro path inválido
    EXPECT_THROW(buscar_arquivo_recursivo("non_existent_folder", "test.txt"), std::invalid_argument);
    
    fs::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
