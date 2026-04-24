#include "formatters/sexpr_formatter.hpp"
#include "utils/sexpr.hpp"
#include "engines/variable_tracker_engine.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <string>

namespace formatters {

    // ==========================================
    // Opções de formatação
    // ==========================================

    SExprFormatter::Opts SExprFormatter::Opts::from(const sexpr::Node& n) {
        Opts o;
        if (!n.is_list()) return o;
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

    bool SExprFormatter::supports_option(const std::string& option) const {
        static const std::vector<std::string> supported = {
            "pretty", "compact", "metadata"
        };
        return std::find(supported.begin(), supported.end(), option) != supported.end();
    }

    std::string SExprFormatter::serialize_results(
        const std::vector<engines::SearchResult>& results,
        std::string_view result_type,
        const Opts& opts
    ) const {
        auto root = sexpr::form(std::string("scout:") + std::string(result_type));
        if (opts.metadata) {
            root.kv("timestamp", sexpr::string(sexpr::current_timestamp()));
            root.kv("version",   sexpr::string("1.1")); // Nível 16
        }
        root.kv("total", sexpr::integer(results.size()));
        auto result_list = sexpr::list();
        for (const auto& r : results) result_list.push(r.to_sexpr());
        root.kv("results", std::move(result_list));
        bool pretty = opts.pretty && !opts.compact;
        return root.to_string(pretty);
    }

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

    // ==========================================
    // Causal Intent Serializer (CIS) - Nível 16
    // ==========================================

    /**
     * @brief Traduz eventos de Taint/Alias em S-Expressions semânticas para IA.
     */
    class CausalIntentSerializer {
        int handle_counter_ = 0;
        std::unordered_map<std::string, std::string> reg_to_handle_;

    public:
        std::string serialize(const std::vector<engines::VariableEvent>& events) {
            auto root = sexpr::form("scout:causal-report");
            auto chain = sexpr::list();

            for (const auto& ev : events) {
                // Saliency Filter: Poda de ruído Dalvik
                if (ev.action == "SINK_LEAK" || ev.action == "TAINT_PROP" || 
                    ev.action == "STORE" || ev.extra == "IMPLICIT" || ev.action == "EES_OPAQUE_ENTRY") {
                    
                    auto node = sexpr::form("step");
                    node.kv("type", sexpr::string(std::string(ev.action)));
                    node.kv("loc", sexpr::string(std::string(ev.method) + ":" + std::to_string(ev.line)));
                    node.kv("handle", sexpr::string(get_handle(ev.reg)));
                    node.kv("target", sexpr::string(std::string(ev.target)));
                    
                    if (!ev.extra.empty()) {
                        node.kv("ctx", sexpr::string(std::string(ev.extra)));
                    }
                    chain.push(node);
                }
            }
            root.kv("intent-chain", std::move(chain));
            return root.to_string(true); // Sempre pretty para IA entender melhor
        }

    private:
        std::string get_handle(std::string_view reg) {
            std::string r(reg);
            if (reg_to_handle_.count(r)) return reg_to_handle_[r];
            return reg_to_handle_[r] = "h" + std::to_string(++handle_counter_);
        }
    };

    // Função global de interface para o motor de análise
    std::string format_causal_intent(const std::vector<engines::VariableEvent>& events) {
        CausalIntentSerializer cis;
        return cis.serialize(events);
    }

} // namespace formatters
