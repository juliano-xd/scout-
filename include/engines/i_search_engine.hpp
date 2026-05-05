#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include "../utils/sexpr.hpp"

namespace core { class AnalysisContext; }

namespace engines {

    /**
     * @brief Resultado padronizado de busca.
     * Estrutura nativa — sem dependência de JSON.
     */
    struct SearchResult {
        std::filesystem::path     file_path;
        std::size_t               line_number   = 0;
        std::string               line_content;
        std::string               context;
        std::string               engine_name;
        std::chrono::microseconds search_time{0};

        /// Serializa diretamente para um nó S-Expression
        sexpr::Node to_sexpr() const {
            auto n = sexpr::form("result");
            n.kv("file",       sexpr::string(file_path.string()));
            n.kv("line",       sexpr::integer(static_cast<long long>(line_number)));
            n.kv("content",    sexpr::string(line_content));
            n.kv("context",    sexpr::string(context));
            n.kv("engine",     sexpr::string(engine_name));
            if (search_time.count() > 0)
                n.kv("time_us", sexpr::integer(static_cast<long long>(search_time.count())));
            return n;
        }
    };

    /**
     * @brief Configuração comum para todos os motores de busca.
     */
    struct SearchConfig {
        std::string              query;
        std::string              search_type   = "string";
        std::size_t              max_results   = 1000;
        bool                     case_sensitive = false;
        std::vector<std::string> include_dirs;
        std::vector<std::string> exclude_dirs;
        bool                     include_system = false;
        int                      search_depth   = 1;
        std::string              direction      = "both";        // For XREF
        std::vector<std::string> filter_opcodes;                 // For XREF
        std::string              var_name;
    };

    /**
     * @brief Estatísticas de performance do motor.
     */
    struct EngineStats {
        std::size_t                   files_scanned  = 0;
        std::size_t                   matches_found  = 0;
        std::chrono::milliseconds     total_time{0};
        std::size_t                   memory_peak_bytes = 0;

        sexpr::Node to_sexpr() const {
            auto n = sexpr::form("stats");
            n.kv("files_scanned",  sexpr::integer(files_scanned));
            n.kv("matches_found",  sexpr::integer(matches_found));
            n.kv("total_time_ms",  sexpr::integer(static_cast<long long>(total_time.count())));
            return n;
        }
    };

    /**
     * @brief Interface abstrata base para motores de busca.
     */
    class ISearchEngine {
    public:
        virtual ~ISearchEngine() = default;

        virtual std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) = 0;

        virtual std::string name()        const = 0;
        virtual std::string description() const = 0;
        virtual EngineStats get_stats()   const = 0;
        virtual bool supports_config(const SearchConfig& config) const = 0;
    };

} // namespace engines
