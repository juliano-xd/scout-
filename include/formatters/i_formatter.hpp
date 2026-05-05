#pragma once

#include <string>
#include <vector>
#include <ostream>
#include "../utils/sexpr.hpp"
#include "../engines/i_search_engine.hpp"

namespace formatters {

    /**
     * @brief Interface base para formatadores de saída do Scout++.
     *
     * O programa opera nativamente em S-Expression. Esta interface recebe
     * estruturas nativas (SearchResult, sexpr::Node) — nunca JSON intermediário.
     *
     * Atualmente a única implementação é SExprFormatter.
     */
    class IFormatter {
    public:
        virtual ~IFormatter() = default;

        // --- Métodos principais ---

        /// Formata lista de resultados de busca
        virtual std::string format_search_results(
            const std::vector<engines::SearchResult>& results,
            const sexpr::Node& options = sexpr::nil()
        ) const = 0;

        /// Formata resultados de XREF (mesma estrutura, semântica diferente)
        virtual std::string format_xref_results(
            const std::vector<engines::SearchResult>& results,
            const sexpr::Node& options = sexpr::nil()
        ) const = 0;

        /// Formata um nó S-Expression arbitrário (status, erros, manifesto, etc.)
        virtual std::string format(
            const sexpr::Node& data,
            const sexpr::Node& options = sexpr::nil()
        ) const = 0;

        /// Escreve diretamente num stream de saída
        virtual void write(
            std::ostream& os,
            const sexpr::Node& data,
            const sexpr::Node& options = sexpr::nil()
        ) const {
            os << format(data, options);
        }

        // --- Metadados do formatador ---

        virtual std::string name()              const = 0;
        virtual std::string default_extension() const = 0;
        virtual bool supports_option(const std::string& option) const = 0;
        virtual int  priority()                 const { return 0; }
    };

} // namespace formatters
