#include <gtest/gtest.h>
#include "engines/manifest_engine.hpp"
#include <fstream>

using namespace engines;

TEST(ManifestAudit, ParseManifest) {
    auto root = std::filesystem::absolute("manifest_audit_root");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    
    {
        std::ofstream out(root / "AndroidManifest.xml");
        out << "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" package=\"com.test.app\">\n"
            << "  <uses-permission android:name=\"android.permission.INTERNET\" />\n"
            << "  <application android:debuggable=\"true\" android:allowBackup=\"true\">\n"
            << "    <activity android:name=\".MainActivity\" android:exported=\"true\">\n"
            << "      <intent-filter>\n"
            << "        <action android:name=\"android.intent.action.MAIN\" />\n"
            << "      </intent-filter>\n"
            << "    </activity>\n"
            << "    <service android:name=\".MyService\" />\n" // Not exported explicitly, no intent-filter
            << "    <receiver android:name=\".MyReceiver\">\n" // Implicitly exported via intent-filter
            << "      <intent-filter>\n"
            << "        <action android:name=\"com.test.ACTION\" />\n"
            << "      </intent-filter>\n"
            << "    </receiver>\n"
            << "  </application>\n"
            << "</manifest>\n";
    }
    
    ManifestEngine engine;
    auto info = engine.parse_manifest(root / "AndroidManifest.xml");
    
    EXPECT_EQ(info.package, "com.test.app");
    ASSERT_EQ(info.permissions.size(), 1);
    EXPECT_EQ(info.permissions[0], "android.permission.INTERNET");
    
    // Check security alerts
    bool debuggable = false, backup = false;
    for(auto& a : info.security_alerts) {
        if(a == "debuggable-enabled") debuggable = true;
        if(a == "allowBackup-enabled") backup = true;
    }
    EXPECT_TRUE(debuggable);
    EXPECT_TRUE(backup);
    
    // Check components
    bool main_act = false, svc = false, rcv = false;
    for(auto& c : info.components) {
        if(c.name == ".MainActivity") {
            main_act = true;
            EXPECT_EQ(c.type, "activity");
            EXPECT_TRUE(c.exported);
            ASSERT_FALSE(c.intent_filters.empty());
            EXPECT_EQ(c.intent_filters[0], "android.intent.action.MAIN");
        }
        if(c.name == ".MyService") {
            svc = true;
            EXPECT_FALSE(c.exported);
        }
        if(c.name == ".MyReceiver") {
            rcv = true;
            EXPECT_TRUE(c.exported); // Implicitly exported
        }
    }
    EXPECT_TRUE(main_act);
    EXPECT_TRUE(svc);
    EXPECT_TRUE(rcv);
    
    std::filesystem::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
