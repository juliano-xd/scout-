#include "cli/parser.hpp"
#include "engines/register_engines.hpp"
#include "utils/sexpr.hpp"
#include <iostream>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    try {
        // Parse de argumentos
        auto config_opt = cli::ScoutConfig::parse(argc, argv);
        if (!config_opt) return 0;

        const auto& config = *config_opt;
        fs::path dir = config.path.value_or(fs::current_path().string());

        // Registrar motores e formatador
        scout::register_all_components();

        // Obter formatador (sempre sexpr)
        auto formatter = scout::create_formatter("sexpr");
        if (!formatter) {
            std::cerr << sexpr::make_error("Falha ao inicializar formatador sexpr").to_string() << "\n";
            return 1;
        }

        // ==========================================
        // Módulo de busca (Classes / Conteúdo)
        // ==========================================
        std::optional<std::string> search_query = config.search;
        if (!search_query && !config.positional_args.empty())
            search_query = config.positional_args[0];

        if (search_query) {
            std::string engine_name = scout::map_search_type_to_engine(config.search_type);
            auto engine = scout::create_engine(engine_name);

            if (engine) {
                engines::SearchConfig scfg;
                scfg.query       = *search_query;
                scfg.max_results = static_cast<std::size_t>(config.search_max);
                auto results = engine->search(dir, scfg);
                std::cout << formatter->format_search_results(results) << "\n";
            } else {
                // Fallback: busca genérica de arquivo por nome
                for (const auto& entry : fs::recursive_directory_iterator(
                        dir, fs::directory_options::skip_permission_denied)) {
                    if (entry.is_regular_file() &&
                        entry.path().filename().string() == *search_query) {
                        engines::SearchResult res;
                        res.file_path   = fs::relative(entry.path(), fs::current_path());
                        res.engine_name = "generic";
                        std::cout << formatter->format_search_results({res}) << "\n";
                        break;
                    }
                }
            }
        }

        // ==========================================
        // Módulo XREF
        // ==========================================
        if (config.xref) {
            auto engine = scout::create_engine("xref");
            if (engine) {
                engines::SearchConfig xcfg;
                xcfg.query = *config.xref;
                auto results = engine->search(dir, xcfg);
                std::cout << formatter->format_xref_results(results) << "\n";
            }
        }

        if (config.cfg) {
            auto engine = scout::create_engine("cfg");
            if (engine) {
                engines::SearchConfig cfg;
                cfg.query = *config.cfg;
                auto results = engine->search(dir, cfg);
                std::cout << formatter->format_search_results(results) << "\n";
            }
        }

        if (config.resource_map || config.find_resource) {
            auto engine = scout::create_engine("resource_map");
            if (engine) {
                engines::SearchConfig rcfg;
                rcfg.query = config.find_resource.value_or("");
                auto results = engine->search(dir, rcfg);
                std::cout << formatter->format_search_results(results) << "\n";
            }
        }

        // ==========================================
        // Módulos ainda não implementados → saída informativa em sexpr
        // ==========================================
        auto emit_pending = [&](std::string_view module) {
            auto n = sexpr::form("scout:pending");
            n.kv("module",  sexpr::string(module));
            n.kv("status",  sexpr::string("not_implemented"));
            std::cout << formatter->format(n) << "\n";
        };

        if (config.manifest)          emit_pending("manifest");
        if (config.scan)              emit_pending("scan");
        if (config.hook)              emit_pending("hook");
        if (config.frida)             emit_pending("frida");
        if (config.detect_obfuscation) emit_pending("detect_obfuscation");


        // Verbose
        if (config.verbose) {
            auto n = sexpr::form("scout:info");
            n.kv("dir",     sexpr::string(dir.string()));
            n.kv("engines", sexpr::string(
                []{
                    auto engines = scout::list_available_engines();
                    std::string s;
                    for (auto& e : engines) { if (!s.empty()) s += ','; s += e; }
                    return s;
                }()
            ));
            std::cout << formatter->format(n) << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << sexpr::make_error(e.what()).to_string() << "\n";
        return 1;
    }

    return 0;
}