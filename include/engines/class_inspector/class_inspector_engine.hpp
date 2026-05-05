#pragma once

#include "../i_search_engine.hpp"
#include "../../core/analysis_context.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace engines {

    /**
     * @brief Motor de introspecção estrutural de classes Smali.
     * Extrai DNA da classe: métodos, campos, herança e interfaces.
     */
    class ClassInspectorEngine : public ISearchEngine {
    public:
        ClassInspectorEngine() = default;
        ~ClassInspectorEngine() override = default;

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "class_inspector"; }
        std::string description() const override {
            return "Extrai a estrutura detalhada (DNA) de uma classe Smali.";
        }

        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override {
            return true;
        }

    public:
        struct ClassStructure {
            std::string name;
            std::string super;
            std::vector<std::string> interfaces;
            std::vector<std::string> fields;
            std::vector<std::string> methods;
        };

        ClassStructure inspect_file(const std::filesystem::path& path);
        sexpr::Node serialize_structure(const ClassStructure& info);

    private:
        EngineStats stats_;
    };

    std::unique_ptr<ISearchEngine> create_class_inspector_engine();

} // namespace engines
