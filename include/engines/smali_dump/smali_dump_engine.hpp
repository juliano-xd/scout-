#pragma once
#include "../i_search_engine.hpp"
#include <memory>

namespace engines {

    class SmaliDumpEngine : public ISearchEngine {
    public:
        SmaliDumpEngine();

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "smali_dump"; }
        std::string description() const override { return "Dumps Smali AST as S-expression"; }
        EngineStats get_stats() const override { return stats_; }
        bool supports_config(const SearchConfig& config) const override {
            return !config.query.empty();
        }

    private:
        EngineStats stats_;
    };

    std::unique_ptr<ISearchEngine> create_smali_dump_engine();

} // namespace engines
