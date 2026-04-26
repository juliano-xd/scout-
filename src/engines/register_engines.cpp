#include "engines/register_engines.hpp"
#include "engines/engine_registry.hpp"
#include "formatters/formatter_registry.hpp"
#include "engines/class_search_engine.hpp"
#include "engines/content_search_engine.hpp"
#include "engines/xref_search_engine.hpp"
#include "engines/resource_mapping_engine.hpp"
#include "engines/cfg_engine.hpp"
#include "engines/manifest_engine.hpp"
#include "engines/class_inspector_engine.hpp"
#include "engines/ui_mapper_engine.hpp"
#include "engines/deobf_engine.hpp"
#include "engines/variable_tracker_engine.hpp"
#include "engines/smali_dump_engine.hpp"
#include "formatters/sexpr_formatter.hpp"

namespace scout {

    namespace {
        static engines::EngineRegistrar<engines::ClassSearchEngine>   _reg_class("class");
        static engines::EngineRegistrar<engines::ContentSearchEngine> _reg_content("content");
        static engines::EngineRegistrar<engines::XrefSearchEngine>    _reg_xref("xref");
        static engines::EngineRegistrar<engines::ResourceMappingEngine> _reg_res("resource_map");
        static engines::EngineRegistrar<engines::CFGEngine>           _reg_cfg("cfg");
        static engines::EngineRegistrar<engines::ManifestEngine>      _reg_manifest("manifest");
        static engines::EngineRegistrar<engines::ClassInspectorEngine> _reg_insp("class_inspector");
        static engines::EngineRegistrar<engines::UiMapperEngine>      _reg_ui("ui_mapper");
        static engines::EngineRegistrar<engines::DeobfEngine>         _reg_deobf("deobf");
        static engines::EngineRegistrar<engines::VariableTrackerEngine> _reg_track("track_var");
        static engines::EngineRegistrar<engines::SmaliDumpEngine>     _reg_dump("smali_dump");
        static formatters::FormatterRegistrar<formatters::SExprFormatter> _reg_sexpr("sexpr");
    }

    bool register_all_components() {
        try {
            auto& er = engines::EngineRegistry::instance();
            auto& fr = formatters::FormatterRegistry::instance();
            return er.has_engine("class")   &&
                   er.has_engine("content") &&
                   er.has_engine("xref")    &&
                   er.has_engine("resource_map") &&
                   er.has_engine("cfg") &&
                   er.has_engine("manifest") &&
                   er.has_engine("class_inspector") &&
                   er.has_engine("ui_mapper") &&
                   er.has_engine("deobf") &&
                   er.has_engine("track_var") &&
                   er.has_engine("smali_dump") &&
                   fr.has_formatter("sexpr");
        } catch (...) {
            return false;
        }
    }

    std::vector<std::string> list_available_engines() {
        return engines::EngineRegistry::instance().list_engines();
    }

    std::unique_ptr<engines::ISearchEngine> create_engine(const std::string& name) {
        return engines::EngineRegistry::instance().create(name);
    }

    std::unique_ptr<formatters::IFormatter> create_formatter(const std::string& name) {
        return formatters::FormatterRegistry::instance().create(name);
    }

    std::unique_ptr<formatters::IFormatter> create_default_formatter() {
        return formatters::FormatterRegistry::instance().create("sexpr");
    }

    bool has_engine(const std::string& name) {
        return engines::EngineRegistry::instance().has_engine(name);
    }

    std::string map_search_type_to_engine(const std::string& search_type, const std::string& query) {
        if (search_type == "class" ||
            (query.starts_with('L') && query.ends_with(';')))
            return "class";
        if (search_type == "regex"   || search_type == "string" ||
            search_type == "integer" || search_type == "number")
            return "content";
        if (search_type == "xref"   || search_type == "cross-reference")
            return "xref";
        return search_type;
    }

} // namespace scout
