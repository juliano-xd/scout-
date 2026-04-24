#pragma once

#include "engines/i_search_engine.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

namespace engines {

    /**
     * @brief Evento de rastreamento de variável.
     */
    struct VariableEvent {
        std::string_view method;
        int line;
        std::string_view reg;
        std::string_view action;
        std::string_view target;
        std::string_view extra;
    };

    /**
     * @brief Sumário de retorno de um método.
     */
    struct MethodSummary {
        bool return_tainted = false;
        std::unordered_set<std::string_view> return_obj_fields;
    };

    /**
     * @brief Chave para o cache de análise.
     */
    struct CacheKey {
        std::string_view method_sig;
        uint64_t reg_mask;

        bool operator==(const CacheKey& other) const {
            return method_sig == other.method_sig && reg_mask == other.reg_mask;
        }
    };

    struct CacheKeyHash {
        std::size_t operator()(const CacheKey& k) const {
            return std::hash<std::string_view>{}(k.method_sig) ^ (std::hash<uint64_t>{}(k.reg_mask) << 1);
        }
    };

    /**
     * @brief Motor de análise de Taint sensível ao objeto e ao caminho.
     */
    class VariableTrackerEngine : public ISearchEngine {
    public:
        struct TrackingState {
            std::string_view current_method;
            uint64_t active_regs = 0;
            std::unordered_map<int, std::unordered_set<std::string_view>> obj_taint_map;
            std::vector<int> control_taint_stack; // Level 15: Sombra de PC
            int depth = 0;
            MethodSummary last_call_summary;
        };

        VariableTrackerEngine();

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "track_var"; }
        std::string description() const override { return "Aero-Taint Nível 14: Path & Object Sensitive Worklist Solver."; }
        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override;

    private:
        MethodSummary track_recursive(
            core::AnalysisContext& ctx,
            TrackingState& state,
            std::vector<VariableEvent>& events,
            const SearchConfig& config
        );

        void process_method_internal(
            std::string_view body,
            TrackingState& state,
            TrackingState& ex_out,
            std::vector<VariableEvent>& events,
            core::AnalysisContext& ctx,
            const SearchConfig& config,
            std::string_view method_name,
            int depth
        );

        static int reg_to_bit(std::string_view reg) {
            if (reg.empty()) return -1;
            char prefix = reg[0];
            try {
                int val = std::stoi(std::string(reg.substr(1)));
                if (prefix == 'v') return (val >= 0 && val < 32) ? val : -1;
                if (prefix == 'p') return (val >= 0 && val < 16) ? 32 + val : -1;
            } catch (...) {}
            return -1;
        }

        static std::string_view bit_to_reg_sv(int bit);
        std::string_view pool_string(std::string_view s);
        bool is_sanitizer(std::string_view target);
        bool is_transform(std::string_view target);

        std::unordered_set<std::string> string_pool_;
        std::unordered_map<CacheKey, std::pair<std::vector<VariableEvent>, MethodSummary>, CacheKeyHash> analysis_cache_;
        EngineStats stats_;
    };

} // namespace engines
