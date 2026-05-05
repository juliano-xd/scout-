#include "../../../include/engines/ui_mapper/ui_mapper_engine.hpp"
#include "../../../include/engines/resource_mapping/resource_mapping_engine.hpp"
#include "../../../include/engines/xref_search/xref_search_engine.hpp"
#include <chrono>

namespace engines {

    std::vector<SearchResult> UiMapperEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto& root_dir = ctx.root_dir();
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;

        if (config.query.empty()) return results;

        // 1. Resolver o ID (se for nome, pegar o hex; se for hex, confirmar)
        std::string target_id = config.query;
        if (!target_id.starts_with("0x")) {
            auto res_engine = create_resource_mapping_engine();
            SearchConfig rcfg;
            rcfg.query = target_id;
            auto res_matches = res_engine->search(ctx, rcfg);
            if (!res_matches.empty()) {
                target_id = res_matches[0].context; // ResourceMappingEngine coloca o ID no context
            }
        }

        // 2. Usar o motor de XREF para encontrar quem usa esse ID
        auto xref_engine = create_xref_search_engine();
        SearchConfig xcfg = config;
        xcfg.query = target_id;
        xcfg.direction = "callers";

        auto xref_results = xref_engine->search(ctx, xcfg);

        for (auto& res : xref_results) {
            res.engine_name = name();
            results.push_back(std::move(res));
        }

        auto end = std::chrono::high_resolution_clock::now();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        stats_.files_scanned = xref_engine->get_stats().files_scanned;
        stats_.matches_found = results.size();

        return results;
    }

    std::unique_ptr<ISearchEngine> create_ui_mapper_engine() {
        return std::make_unique<UiMapperEngine>();
    }

} // namespace engines
