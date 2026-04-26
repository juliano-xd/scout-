#include <gtest/gtest.h>
#include "core/scanner.hpp"
#include <fstream>
#include <atomic>
#include <filesystem>

namespace fs = std::filesystem;
using namespace core;

// 1. ClassInfo Parsing
TEST(ScannerAudit, ClassInfoParse) {
    auto info1 = ClassInfo::parse("Lcom/app/MainActivity;");
    EXPECT_EQ(info1.package_path, "com/app");
    EXPECT_EQ(info1.class_name, "MainActivity");
    EXPECT_EQ(info1.file_name, "MainActivity.smali");
    
    auto info2 = ClassInfo::parse("android.os.Bundle");
    EXPECT_EQ(info2.package_path, "android/os");
    EXPECT_EQ(info2.class_name, "Bundle");
    
    auto info3 = ClassInfo::parse("LMain;");
    EXPECT_EQ(info3.package_path, "");
    EXPECT_EQ(info3.class_name, "Main");
}

// 2. Path Filtering
TEST(ScannerAudit, PathFiltering) {
    fs::path p = "smali/com/app/MainActivity.smali";
    
    EXPECT_TRUE(is_path_filtered(p, {"smali"}, {}));
    EXPECT_FALSE(is_path_filtered(p, {"other"}, {}));
    EXPECT_FALSE(is_path_filtered(p, {}, {"app"}));
    EXPECT_TRUE(is_path_filtered(p, {}, {"other"}));
    EXPECT_TRUE(is_path_filtered(p, {"smali"}, {"other"}));
    EXPECT_FALSE(is_path_filtered(p, {"smali"}, {"com"}));
}

// 3. Find Class File
TEST(ScannerAudit, FindClassFile) {
    fs::path root = "scanner_audit_root";
    fs::remove_all(root);
    fs::create_directories(root / "smali_classes2/com/app");
    fs::create_directories(root / "custom/pkg");
    
    {
        std::ofstream(root / "smali_classes2/com/app/Main.smali");
        std::ofstream(root / "custom/pkg/Extra.smali");
    }
    
    auto res1 = find_class_file(root, "Lcom/app/Main;");
    ASSERT_TRUE(res1.has_value());
    EXPECT_TRUE(res1->generic_string().find("smali_classes2") != std::string::npos);
    
    auto res2 = find_class_file(root, "Lcustom/pkg/Extra;");
    ASSERT_TRUE(res2.has_value());
    EXPECT_TRUE(res2->generic_string().find("custom/pkg") != std::string::npos);
    
    fs::remove_all(root);
}

// 4. Parallel Scan
TEST(ScannerAudit, ScanFilesParallel) {
    fs::path root = "scanner_audit_par";
    fs::remove_all(root);
    fs::create_directories(root / "d1");
    fs::create_directories(root / "d2");
    
    for(int i=0; i<100; ++i) {
        std::ofstream(root / ("d1/f" + std::to_string(i) + ".smali"));
        std::ofstream(root / ("d2/g" + std::to_string(i) + ".smali"));
    }
    
    std::atomic<int> count = 0;
    scan_files(root, [&](const fs::path& p) {
        count++;
    }, {"d1"}, {});
    
    EXPECT_EQ(count, 100);
    
    count = 0;
    scan_files(root, [&](const fs::path& p) {
        count++;
    });
    EXPECT_EQ(count, 200);
    
    fs::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
