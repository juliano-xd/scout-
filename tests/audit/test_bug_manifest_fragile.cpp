#include <gtest/gtest.h>
#include "engines/manifest_engine.hpp"
#include "core/analysis_context.hpp"
#include <fstream>
#include <filesystem>

using namespace engines;

TEST(BugInvestigation, ManifestFragileParsing) {
    auto root = std::filesystem::absolute("bug_root_manifest");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    
    {
        std::ofstream out(root / "AndroidManifest.xml");
        out << "<?xml version='1.0' encoding='utf-8'?>\n"
            << "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" package=\"com.bug.test\">\n"
            << "    <application android:debuggable = 'true' android:allowBackup=\"true\">\n"
            << "        <activity android:name='com.bug.MainActivity'>\n"
            << "        </activity>\n"
            << "    </application>\n"
            << "</manifest>\n";
    }
    
    core::AnalysisContext ctx(root);
    ManifestEngine engine;
    SearchConfig config;
    
    auto results = engine.search(ctx, config);
    ASSERT_EQ(results.size(), 1);
    
    // Check if debuggable was found (it uses get_attr internally in parse_manifest)
    // We need to check the serialized context
    EXPECT_TRUE(results[0].context.find(":debuggable \"true\"") != std::string::npos);
    EXPECT_TRUE(results[0].context.find(":package \"com.bug.test\"") != std::string::npos);
    
    std::filesystem::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
