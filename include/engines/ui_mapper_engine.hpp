#pragma once

#include "i_search_engine.hpp"
#include "core/analysis_context.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace engines {

    /**
     * @brief Motor de mapeamento de UI para Código.
     * Encontra a relação entre IDs de recursos XML e manipuladores Smali.
     */
    class UiMapperEngine : public ISearchEngine {
    public:
        UiMapperEngine() = default;
        ~UiMapperEngine() override = default;

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "ui_mapper"; }
        std::string description() const override {
            return "Vincula IDs de layouts XML a classes Smali (OnClickListener, etc).";
        }

        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override {
            return true;
        }

    private:
        EngineStats stats_;

        struct UiBinding {
            std::string resource_name;
            std::string resource_id;
            std::string smali_class;
        };

        std::vector<UiBinding> perform_mapping(const std::filesystem::path& root_dir, const std::string& target_id);
    };

    std::unique_ptr<ISearchEngine> create_ui_mapper_engine();

} // namespace engines
