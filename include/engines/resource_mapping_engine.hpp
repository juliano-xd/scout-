#pragma once

#include "i_search_engine.hpp"
#include "core/analysis_context.hpp"
#include <map>
#include <string>
#include <filesystem>

namespace engines {

    /**
     * @brief Engine para mapeamento de recursos Android (ID -> Nome).
     */
    class ResourceMappingEngine : public ISearchEngine {
    public:
        ResourceMappingEngine() = default;
        ~ResourceMappingEngine() override = default;

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "resource_map"; }
        std::string description() const override {
            return "Mapeia IDs de recursos hexadecimais (0x7f...) para seus nomes simbólicos.";
        }

        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override { return true; }

        /**
         * @brief Carrega o mapa de recursos a partir do diretório raiz.
         */
        void load_map(const std::filesystem::path& root_dir);

        void scan_public_xml(const std::filesystem::path& path);
        void scan_r_smali(const std::filesystem::path& path);
        std::string resolve_id(uint32_t id) const;

    private:
        EngineStats stats_;
        std::map<uint32_t, std::string> id_to_name_;
        bool loaded_ = false;
    };

    std::unique_ptr<ISearchEngine> create_resource_mapping_engine();

} // namespace engines
