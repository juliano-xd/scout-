#pragma once

#include "engines/engine_registry.hpp"
#include "formatters/formatter_registry.hpp"
#include "engines/class_search_engine.hpp"
#include "engines/content_search_engine.hpp"
#include "engines/xref_search_engine.hpp"
#include "engines/resource_mapping_engine.hpp"
#include "engines/cfg_engine.hpp"
#include "formatters/sexpr_formatter.hpp"
#include <memory>

namespace scout {

    /**
     * @brief Registro automático de motores e formatador S-Expression.
     *
     * O Scout++ opera nativamente em S-Expression. O único formatador registrado
     * é o SExprFormatter — não há suporte a JSON/Text/YAML como saída.
     */

    namespace {
        static engines::EngineRegistrar<engines::ClassSearchEngine>   _reg_class("class");
        static engines::EngineRegistrar<engines::ContentSearchEngine> _reg_content("content");
        static engines::EngineRegistrar<engines::XrefSearchEngine>    _reg_xref("xref");
        static engines::EngineRegistrar<engines::ResourceMappingEngine> _reg_res("resource_map");
        static engines::EngineRegistrar<engines::CFGEngine>           _reg_cfg("cfg");
        static formatters::FormatterRegistrar<formatters::SExprFormatter> _reg_sexpr("sexpr");
    }

    /**
     * @brief Inicializa todos os componentes (motores + formatador).
     * @return true se registrados com sucesso
     */
    inline bool register_all_components() {
        try {
            auto& er = engines::EngineRegistry::instance();
            auto& fr = formatters::FormatterRegistry::instance();
            return er.has_engine("class")   &&
                   er.has_engine("content") &&
                   er.has_engine("xref")    &&
                   er.has_engine("resource_map") &&
                   er.has_engine("cfg") &&
                   fr.has_formatter("sexpr");
        } catch (...) {
            return false;
        }
    }



    inline std::vector<std::string> list_available_engines() {
        return engines::EngineRegistry::instance().list_engines();
    }

    inline std::unique_ptr<engines::ISearchEngine> create_engine(const std::string& name) {
        return engines::EngineRegistry::instance().create(name);
    }

    inline std::unique_ptr<formatters::IFormatter> create_formatter(const std::string& name) {
        return formatters::FormatterRegistry::instance().create(name);
    }

    inline std::unique_ptr<formatters::IFormatter> create_default_formatter() {
        return formatters::FormatterRegistry::instance().create("sexpr");
    }

    inline bool has_engine(const std::string& name) {
        return engines::EngineRegistry::instance().has_engine(name);
    }

    /**
     * @brief Mapeia tipo de busca CLI para o motor correspondente.
     */
    inline std::string map_search_type_to_engine(const std::string& search_type) {
        if (search_type == "class" ||
            (search_type.starts_with('L') && search_type.ends_with(';')))
            return "class";
        if (search_type == "regex"   || search_type == "string" ||
            search_type == "integer" || search_type == "number")
            return "content";
        if (search_type == "xref"   || search_type == "cross-reference")
            return "xref";
        return search_type;
    }

} // namespace scout