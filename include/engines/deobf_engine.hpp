#pragma once

#include "i_search_engine.hpp"
#include "core/analysis_context.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>

namespace engines {

    /**
     * @brief Motor de detecção de obfuscação de strings.
     * Identifica strings suspeitas (Base64, XOR) e seus decoders.
     */
    class DeobfEngine : public ISearchEngine {
    public:
        DeobfEngine() = default;
        ~DeobfEngine() override = default;

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "deobf"; }
        std::string description() const override {
            return "Detecta strings obfuscadas e localiza algoritmos de decodificação.";
        }

        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override {
            return true;
        }

        bool is_suspicious(std::string_view str);
        std::string decode_simple(std::string_view str);

    private:
        EngineStats stats_;
        std::mutex stats_mutex_;
    };

    std::unique_ptr<ISearchEngine> create_deobf_engine();

} // namespace engines
