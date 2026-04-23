#pragma once

#include "i_formatter.hpp"
#include "utils/sexpr.hpp"

namespace formatters {

    /**
     * @brief Formatter nativo S-Expression para o Scout++.
     *
     * Serializa estruturas nativas (SearchResult, sexpr::Node) diretamente
     * para S-Expression. Não há conversão JSON → SExpr em nenhuma etapa.
     *
     * Opções (passadas como sexpr::Node lista de keywords):
     *   :pretty true/false  — indentação multi-linha
     *   :compact true/false — sem espaços extras (override de pretty)
     *   :metadata true/false — inclui :timestamp e :version no header (padrão: true)
     */
    class SExprFormatter : public IFormatter {
    public:
        SExprFormatter()  = default;
        ~SExprFormatter() override = default;

        std::string name()              const override { return "sexpr"; }
        std::string default_extension() const override { return ".sexpr"; }
        bool supports_option(const std::string& option) const override;

        std::string format_search_results(
            const std::vector<engines::SearchResult>& results,
            const sexpr::Node& options = sexpr::nil()
        ) const override;

        std::string format_xref_results(
            const std::vector<engines::SearchResult>& results,
            const sexpr::Node& options = sexpr::nil()
        ) const override;

        std::string format(
            const sexpr::Node& data,
            const sexpr::Node& options = sexpr::nil()
        ) const override;

    private:
        struct Opts {
            bool pretty   = false;
            bool compact  = false;
            bool metadata = true;

            static Opts from(const sexpr::Node& n);
        };

        std::string serialize_results(
            const std::vector<engines::SearchResult>& results,
            std::string_view result_type,
            const Opts& opts
        ) const;
    };

} // namespace formatters