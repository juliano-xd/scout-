#pragma once
#include "engines/i_search_engine.hpp"
#include "core/analysis_context.hpp"
#include "formatters/i_formatter.hpp"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace scout {

    /**
     * @brief Inicializa todos os componentes (motores + formatador).
     */
    bool register_all_components();

    std::vector<std::string> list_available_engines();

    std::unique_ptr<engines::ISearchEngine> create_engine(const std::string& name);

    std::unique_ptr<formatters::IFormatter> create_formatter(const std::string& name);

    std::unique_ptr<formatters::IFormatter> create_default_formatter();

    bool has_engine(const std::string& name);

    std::string map_search_type_to_engine(const std::string& search_type);

} // namespace scout