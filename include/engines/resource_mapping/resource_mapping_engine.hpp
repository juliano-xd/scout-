#pragma once

#include <unordered_map>
#include <string>
#include <string_view>
#include <filesystem>
#include <memory>
#include <vector>
#include <cstdint>

#include "../i_search_engine.hpp"
#include "../../core/analysis_context.hpp"


namespace engines {

    /**
     * @file resource_mapping_engine.hpp
     * @brief Engine para mapeamento de recursos Android (ID -> Nome).
     * @details Arquitetura otimizada para C++26. Substituição de estruturas em árvore
     * por Tabelas Hash contíguas e devoluções Zero-Copy.
     */
    class ResourceMappingEngine final : public ISearchEngine {
    public:
        ResourceMappingEngine() = default;
        ~ResourceMappingEngine() override = default;

        // ==========================================
        // Gestão de Memória Segura (Regra dos 5)
        // ==========================================
        // Bloqueia cópias acidentais massivas de Dicionários Hash
        ResourceMappingEngine(const ResourceMappingEngine&) = delete;
        ResourceMappingEngine& operator=(const ResourceMappingEngine&) = delete;

        // Habilita movimentações lock-free e alocação dinâmica em smart_pointers
        ResourceMappingEngine(ResourceMappingEngine&&) noexcept = default;
        ResourceMappingEngine& operator=(ResourceMappingEngine&&) noexcept = default;

        // ==========================================
        // Interface ISearchEngine Overrides
        // ==========================================
        [[nodiscard]] std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        [[nodiscard]] std::string name() const override { return "resource_map"; }

        [[nodiscard]] std::string description() const override {
            return "Mapeia IDs de recursos hexadecimais (0x7f...) para seus nomes simbólicos.";
        }

        [[nodiscard]] EngineStats get_stats() const override { return stats_; }

        [[nodiscard]] bool supports_config(const SearchConfig& config) const override {
            return true;
        }

        // ==========================================
        // Subsistemas Internos
        // ==========================================

        /**
         * @brief Carrega o mapa de recursos a partir do diretório raiz.
         */
        void load_map(const std::filesystem::path& root_dir);

        void scan_public_xml(const std::filesystem::path& path);

        void scan_r_smali(const std::filesystem::path& path);

        /**
         * @brief Resolve um ID hexadecimal para o seu nome simbólico em O(1).
         * @param id O identificador numérico de 32-bits do recurso Android (ex: 0x7f040001).
         * @return Uma view para o nome original, ou view vazia se não encontrado.
         * @details Devolução Zero-Copy usando std::string_view. Garantia `noexcept` instrui
         * o compilador a ignorar blocos de stack unwinding.
         */
        [[nodiscard]] std::string_view resolve_id(uint32_t id) const noexcept;

    private:
        EngineStats stats_;

        // OPTIMIZAÇÃO: std::unordered_map oferece lookup médio O(1).
        // Em comparação ao std::map (O(log N)), reduzimos o cache-miss factor
        // dramáticamente por não precisarmos saltar entre nós de uma árvore na Heap.
        std::unordered_map<uint32_t, std::string> id_to_name_;

        bool loaded_ = false;
    };

    /**
     * @brief Factory method para instanciar o motor de recursos.
     */
    [[nodiscard]] std::unique_ptr<ISearchEngine> create_resource_mapping_engine();

} // namespace engines
