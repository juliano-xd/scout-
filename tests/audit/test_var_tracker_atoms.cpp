#include <gtest/gtest.h>
#include "engines/variable_tracker_engine.hpp"

using namespace engines;

// 1. PointsToSet SVO & RAII
TEST(VarTrackerAtoms, PointsToSetSVO) {
    PointsToSet pts;
    EXPECT_TRUE(pts.empty());
    
    pts.add(101);
    pts.add(102);
    EXPECT_FALSE(pts.is_large);
    EXPECT_EQ(pts.count, 2);
    EXPECT_TRUE(pts.contains(101));
    
    // Transição para Large
    pts.add(103);
    EXPECT_TRUE(pts.is_large);
    EXPECT_EQ(pts.count, 3);
    EXPECT_TRUE(pts.contains(103));
    
    // Copy Constructor
    PointsToSet pts2 = pts;
    EXPECT_TRUE(pts2.is_large);
    EXPECT_TRUE(pts2.contains(101));
    
    // Assignment
    PointsToSet pts3;
    pts3 = pts;
    EXPECT_TRUE(pts3.contains(102));
    
    pts.clear();
    EXPECT_TRUE(pts.empty());
    EXPECT_FALSE(pts.is_large);
}

// 2. Register Mapping
TEST(VarTrackerAtoms, RegMapping) {
    EXPECT_EQ(VariableTrackerEngine::reg_to_bit("v0"), 0);
    EXPECT_EQ(VariableTrackerEngine::reg_to_bit("v31"), 31);
    EXPECT_EQ(VariableTrackerEngine::reg_to_bit("p0"), 32);
    EXPECT_EQ(VariableTrackerEngine::reg_to_bit("p15"), 47);
    
    EXPECT_EQ(VariableTrackerEngine::reg_to_bit("x0"), -1);
    EXPECT_EQ(VariableTrackerEngine::reg_to_bit(""), -1);
}

// 3. Lattice Merge
TEST(VarTrackerAtoms, StateMerge) {
    VariableTrackerEngine::TrackingState s1, s2;
    s1.active_regs = (1ULL << 0); // v0
    s2.active_regs = (1ULL << 1); // v1
    
    VariableTrackerEngine::merge_states(s1, s2);
    EXPECT_EQ(s1.active_regs, 3ULL); // v0 | v1
    
    s2.obj_taint_map[0].insert("secret");
    VariableTrackerEngine::merge_states(s1, s2);
    EXPECT_TRUE(s1.obj_taint_map[0].count("secret"));
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
