#pragma once

#include "i_search_engine.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace engines {

    /**
     * @brief Bloco básico em um Control Flow Graph.
     */
    struct BasicBlock {
        std::string id;
        size_t start_line;
        size_t end_line;
        std::vector<std::string> instructions;
        std::vector<std::string> successors; // IDs dos blocos sucessores
    };

    /**
     * @brief Motor para análise de Control Flow Graph (CFG).
     */
    class CFGEngine : public ISearchEngine {
    public:
        CFGEngine() = default;
        ~CFGEngine() override = default;

        std::vector<SearchResult> search(
            const std::filesystem::path& root_dir,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "cfg"; }
        std::string description() const override {
            return "Gera o Control Flow Graph (CFG) de um método Smali específico.";
        }

        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override { return true; }

    private:
        EngineStats stats_;

        std::vector<BasicBlock> analyze_method(const std::filesystem::path& file, const std::string& method_sig);
        std::string blocks_to_sexpr(const std::vector<BasicBlock>& blocks);
        std::string blocks_to_dot(const std::vector<BasicBlock>& blocks);
    };

    std::unique_ptr<ISearchEngine> create_cfg_engine();

} // namespace engines
