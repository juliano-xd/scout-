#include <gtest/gtest.h>
#include "formatters/formatter_registry.hpp"

using namespace formatters;

class MockFormatter : public IFormatter {
public:
    std::string format(const std::vector<engines::SearchResult>&) override { return "mock"; }
    std::string name() const override { return "mock"; }
};

TEST(FormatterRegistryAudit, BasicRegistration) {
    auto& reg = FormatterRegistry::instance();
    
    // Test duplicate registration throws
    reg.register_formatter("mock", []() { return std::make_unique<MockFormatter>(); });
    EXPECT_THROW(reg.register_formatter("mock", []() { return std::make_unique<MockFormatter>(); }), std::runtime_error);
    
    EXPECT_TRUE(reg.has_formatter("mock"));
    auto formatter = reg.create("mock");
    ASSERT_NE(formatter, nullptr);
    EXPECT_EQ(formatter->name(), "mock");
    
    auto names = reg.list_formatters();
    bool found = false;
    for(auto& n : names) if(n == "mock") found = true;
    EXPECT_TRUE(found);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
