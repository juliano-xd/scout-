#pragma once

#include "../i_search_engine.hpp"
#include "../../core/analysis_context.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace engines {

    /**
     * @brief Motor de análise do AndroidManifest.xml.
     * Extrai componentes, permissões e configurações de segurança.
     */
    class ManifestEngine : public ISearchEngine {
    public:
        ManifestEngine() = default;
        ~ManifestEngine() override = default;

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "manifest"; }
        std::string description() const override {
            return "Analisa o AndroidManifest.xml para extrair componentes e permissões.";
        }

        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override {
            return true;
        }

    public:
        struct ManifestInfo {
            std::string package;
            std::vector<std::string> permissions;
            struct Component {
                std::string name;
                std::string type; // activity, service, etc.
                bool exported = false;
                std::vector<std::string> intent_filters;
            };
            std::vector<Component> components;
            std::vector<std::string> security_alerts;
        };

        ManifestInfo parse_manifest(const std::filesystem::path& path);
        sexpr::Node serialize_info(const ManifestInfo& info);

    private:
        EngineStats stats_;
    };

    std::unique_ptr<ISearchEngine> create_manifest_engine();

} // namespace engines
