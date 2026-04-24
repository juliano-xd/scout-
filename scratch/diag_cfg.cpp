#include "engines/cfg_engine.hpp"
#include <iostream>

int main() {
    std::string_view smali = R"(.method public test(Ljava/lang/String;)V
    .registers 3
    move-object v0, p1
    invoke-static {v0}, Lcom/test/B;->sink(Ljava/lang/String;)V
    return-void
.end method)";

    auto cfg = engines::CFGEngine::build_cfg(smali);
    std::cout << "Blocks: " << cfg.blocks.size() << std::endl;
    for (const auto& b : cfg.blocks) {
        std::cout << "Block " << b.id << " content:\n[" << b.code_content << "]\n";
        std::cout << "Successors: ";
        for (int s : b.successors) std::cout << s << " ";
        std::cout << "\nPredecessors: ";
        for (int p : b.predecessors) std::cout << p << " ";
        std::cout << "\n-------------------\n";
    }
    return 0;
}
