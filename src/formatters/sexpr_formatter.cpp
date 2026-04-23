#include "formatters/sexpr_formatter.hpp"
#include "utils/sexpr.hpp"
#include <algorithm>

namespace formatters {

    // ==========================================
    // Opções de formatação
    // ==========================================

    SExprFormatter::Opts SExprFormatter::Opts::from(const sexpr::Node& n) {
        Opts o;
        if (!n.is_list()) return o;
        // Percorre pares :key val na lista de opções
        for (std::size_t i = 0; i + 1 < n.items.size(); i += 2) {
            const auto& k = n.items[i];
            const auto& v = n.items[i + 1];
            if (k.kind != sexpr::Node::Kind::Keyword) continue;
            if (k.str_val == "pretty"   && v.kind == sexpr::Node::Kind::Bool) o.pretty   = v.bool_val;
            if (k.str_val == "compact"  && v.kind == sexpr::Node::Kind::Bool) o.compact  = v.bool_val;
            if (k.str_val == "metadata" && v.kind == sexpr::Node::Kind::Bool) o.metadata = v.bool_val;
        }
        return o;
    }

    // ==========================================
    // Opções suportadas
    // ==========================================

    bool SExprFormatter::supports_option(const std::string& option) const {
        static const std::vector<std::string> supported = {
            "pretty", "compact", "metadata"
        };
        return std::find(supported.begin(), supported.end(), option) != supported.end();
    }

    // ==========================================
    // Serialização de resultados
    // ==========================================

    std::string SExprFormatter::serialize_results(
        const std::vector<engines::SearchResult>& results,
        std::string_view result_type,
        const Opts& opts
    ) const {
        // Constrói o nó principal nativamente
        auto root = sexpr::form(std::string("scout:") + std::string(result_type));

        if (opts.metadata) {
            root.kv("timestamp", sexpr::string(sexpr::current_timestamp()));
            root.kv("version",   sexpr::string("1.0"));
        }
        root.kv("total", sexpr::integer(results.size()));

        auto result_list = sexpr::list();
        for (const auto& r : results) {
            result_list.push(r.to_sexpr());
        }
        root.kv("results", std::move(result_list));

        bool pretty = opts.pretty && !opts.compact;
        return root.to_string(pretty);
    }

    // ==========================================
    // Implementações da interface
    // ==========================================

    std::string SExprFormatter::format_search_results(
        const std::vector<engines::SearchResult>& results,
        const sexpr::Node& options
    ) const {
        return serialize_results(results, "search", Opts::from(options));
    }

    std::string SExprFormatter::format_xref_results(
        const std::vector<engines::SearchResult>& results,
        const sexpr::Node& options
    ) const {
        return serialize_results(results, "xref", Opts::from(options));
    }

    std::string SExprFormatter::format(
        const sexpr::Node& data,
        const sexpr::Node& options
    ) const {
        Opts opts = Opts::from(options);
        bool pretty = opts.pretty && !opts.compact;
        return data.to_string(pretty);
    }

} // namespace formatters
