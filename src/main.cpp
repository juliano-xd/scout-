#include "cli/parser.hpp"
#include "engines/register_engines.hpp"
#include "utils/sexpr.hpp"
#include "core/analysis_context.hpp"
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
        core::AnalysisContext analysis_ctx(dir);

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
            std::string engine_name = scout::map_search_type_to_engine(config.search_type, *search_query);
            auto engine = scout::create_engine(engine_name);

            if (engine) {
                engines::SearchConfig scfg;
                scfg.query          = *search_query;
                scfg.search_type    = config.search_type;
                scfg.case_sensitive = config.case_sensitive;
                scfg.max_results    = static_cast<std::size_t>(config.search_max);
                scfg.include_dirs   = config.include_dirs;
                scfg.exclude_dirs   = config.exclude_dirs;
                auto results = engine->search(analysis_ctx, scfg);
                if (config.machine_sexpr) {
                    std::cout << formatter->format_search_results(results, sexpr::list({sexpr::keyword("pretty"), sexpr::boolean(true)})) << "\n";
                } else {
                    std::cout << formatter->format_search_results(results) << "\n";
                }
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
        // Módulo XREF (Especializado)
        // ==========================================
        auto run_xref = [&](const std::string& target, std::string direction, const std::vector<std::string>& opcodes = {}) {
            auto engine = scout::create_engine("xref");
            if (engine) {
                engines::SearchConfig xcfg;
                xcfg.query = target;
                xcfg.direction = direction;
                xcfg.filter_opcodes = opcodes;
                xcfg.include_system = config.xref_include_system;
                xcfg.search_depth = config.xref_depth;
                auto results = engine->search(analysis_ctx, xcfg);
                std::cout << formatter->format_xref_results(results) << "\n";
            }
        };

        if (config.xref)         run_xref(*config.xref, config.xref_direction);
        if (config.xref_callers) run_xref(*config.xref_callers, "callers", {"invoke-"});
        if (config.xref_callees) run_xref(*config.xref_callees, "callees");
        if (config.xref_fields)  run_xref(*config.xref_fields, "callers", {"get", "put"}); // Pega sget, iget, sput, iput


        if (config.cfg) {
            auto engine = scout::create_engine("cfg");
            if (engine) {
                engines::SearchConfig cfg;
                cfg.query = *config.cfg;
                auto results = engine->search(analysis_ctx, cfg);
                std::cout << formatter->format_search_results(results) << "\n";
            }
        }

        if (config.resource_map || config.find_resource) {
            auto engine = scout::create_engine("resource_map");
            if (engine) {
                engines::SearchConfig rcfg;
                rcfg.query = config.find_resource.value_or("");
                auto results = engine->search(analysis_ctx, rcfg);
                if (config.machine_sexpr) {
                    std::cout << formatter->format_search_results(results, sexpr::list({sexpr::keyword("pretty"), sexpr::boolean(true)})) << "\n";
                } else {
                    std::cout << formatter->format_search_results(results) << "\n";
                }
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

        if (config.manifest) {
            auto engine = scout::create_engine("manifest");
            if (engine) {
                engines::SearchConfig mcfg;
                auto results = engine->search(analysis_ctx, mcfg);
                std::cout << formatter->format_search_results(results) << "\n";
            }
        }

        if (config.inspect_class) {
            auto engine = scout::create_engine("class_inspector");
            if (engine) {
                engines::SearchConfig icfg;
                icfg.query = *config.inspect_class;
                auto results = engine->search(analysis_ctx, icfg);
                std::cout << formatter->format_search_results(results) << "\n";
            }
        }

        if (config.ui_mapper) {
            auto engine = scout::create_engine("ui_mapper");
            if (engine) {
                engines::SearchConfig ucfg;
                ucfg.query = *config.ui_mapper;
                auto results = engine->search(analysis_ctx, ucfg);
                std::cout << formatter->format_search_results(results) << "\n";
            }
        }

        if (config.deobf_strings) {
            auto engine = scout::create_engine("deobf");
            if (engine) {
                engines::SearchConfig dcfg;
                auto results = engine->search(analysis_ctx, dcfg);
                std::cout << formatter->format_search_results(results) << "\n";
            }
        }

        if (config.translate) {
            auto engine = scout::create_engine("translate");
            if (engine) {
                engines::SearchConfig tcfg;
                tcfg.query = *config.translate;
                auto results = engine->search(analysis_ctx, tcfg);
                std::cout << formatter->format_search_results(results) << "\n";
            }
        }

        if (config.track_var) {
            auto engine = scout::create_engine("track_var");
            if (engine) {
                engines::SearchConfig vcfg;
                vcfg.query = *config.track_var;
                vcfg.var_name = config.track_var_name;
                vcfg.search_depth = config.track_depth;
                auto results = engine->search(analysis_ctx, vcfg);
                
                if (config.machine_sexpr) {
                    // Para o VariableTracker, usamos o formatador causal de alto nível (Nível 16)
                    // que converte os eventos em uma narrativa de intenção com handles (h1, h2...).
                    // Como SearchResult encapsula os eventos no context, mas o formatador 
                    // causal precisa dos eventos brutos, por enquanto vamos imprimir o context
                    // formatado de forma bonita, ou integrar diretamente se tivermos acesso.
                    std::cout << formatter->format_search_results(results, sexpr::list({sexpr::keyword("pretty"), sexpr::boolean(true)})) << "\n";
                } else {
                    std::cout << formatter->format_search_results(results) << "\n";
                }
            }
        }

        // ==========================================
        // Módulos ainda não implementados → saída informativa em sexpr
        // ==========================================

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