#pragma once

#include "../i_search_engine.hpp"
#include <vector>
#include <string_view>
#include <unordered_map>

namespace engines {

    /**
     * @brief Representa uma aresta de exceção fatorada (FCFG).
     */
    struct ExceptionEdge {
        std::string_view type; // "Ljava/lang/Exception;" ou "" para catchall
        int target_id;
        int priority;
    };

    /**
     * @brief Bloco Básico com suporte a fluxo normal e de exceção.
     */
    struct BasicBlock {
        int id;
        std::string_view code_content;

        // Fluxo Normal
        std::vector<int> successors;
        std::vector<int> predecessors;

        // Fluxo de Exceção
        std::vector<ExceptionEdge> handlers;

        // Metadados de Fluxo e Dominância
        int ipd = -1; // Immediate Post-Dominator (Nível 15)
        bool is_in_try_scope = false;
        bool contains_peis = false;
    };

    struct CFG {
        std::vector<BasicBlock> blocks;
        int entry_block_id = 0;
    };

    /**
     * @brief Especialista em Matemática de Grafos para Dominância.
     */
    class DominatorAnalyzer {
    public:
        static std::unordered_map<int, int> compute_ipds(CFG& cfg);
    };

    /**
     * @brief Motor de construção de Grafos de Controle de Fluxo.
     */
    class CFGEngine : public ISearchEngine {
    public:
        CFGEngine();

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "cfg"; }
        std::string description() const override { return "Aero-CFG Nível 14: Exception-Aware Factored CFG."; }
        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override {
            return config.query.find("->") != std::string::npos;
        }

        // Método estático para uso interno por outros motores
        static CFG build_cfg(std::string_view method_body);

    private:
        EngineStats stats_;
    };

} // namespace engines
