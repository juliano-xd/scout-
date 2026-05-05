#include "../include/cli/parser.hpp"
#include "../include/engines/register_engines.hpp"
#include "../include/utils/sexpr.hpp"
#include "../include/core/analysis_context.hpp"

#include <filesystem>
#include <system_error>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

// Headers modernos C++23/26
#include <print>   // Substitui <iostream> e std::cout para I/O de alta performance
#include <ranges>  // Para manipulação de coleções O(1)

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    try {
        // Parse de argumentos
        auto config_opt = cli::ScoutConfig::parse(argc, argv);
        if (!config_opt) return 0;

        const auto& config = *config_opt;
        const fs::path dir = config.path.value_or(fs::current_path().string());

        // Instancia o contexto central (Carrega a AST do Smali em memória)
        core::AnalysisContext analysis_ctx(dir);

        // Registrar motores e formatador
        scout::register_all_components();

        // Obter formatador (sempre sexpr)
        auto formatter = scout::create_formatter("sexpr");
        if (!formatter) {
            std::println(stderr, "{}", sexpr::make_error("Falha ao inicializar formatador sexpr").to_string());
            return 1;
        }

        // ==========================================
        // Utilitários Locais (Lambdas de C++14/20)
        // ==========================================

        // Lambda genérica para evitar repetição de código na formatação de saída
        auto print_results = [&](const auto& results) {
            if (config.machine_sexpr) {
                auto args = sexpr::list({sexpr::keyword("pretty"), sexpr::boolean(true)});
                std::println("{}", formatter->format_search_results(results, args));
            } else {
                std::println("{}", formatter->format_search_results(results));
            }
        };

        auto emit_pending = [&](std::string_view module) {
            auto n = sexpr::form("scout:pending");
            n.kv("module",  sexpr::string(module));
            n.kv("status",  sexpr::string("not_implemented"));
            std::println("{}", formatter->format(n));
        };

        // ==========================================
        // Módulo de Busca (Classes / Conteúdo)
        // ==========================================
        std::optional<std::string> search_query = config.search;
        if (!search_query && !config.positional_args.empty()) {
            search_query = config.positional_args[0];
        }

        if (search_query) {
            std::string engine_name = scout::map_search_type_to_engine(config.search_type, *search_query);

            if (auto engine = scout::create_engine(engine_name)) {
                engines::SearchConfig scfg;
                scfg.query          = *search_query;
                scfg.search_type    = config.search_type;
                scfg.case_sensitive = config.case_sensitive;
                scfg.max_results    = static_cast<std::size_t>(config.search_max);
                scfg.include_dirs   = config.include_dirs;
                scfg.exclude_dirs   = config.exclude_dirs;

                auto results = engine->search(analysis_ctx, scfg);
                print_results(results);
            }
            else {
                // Fallback: Busca genérica (Blindada contra I/O crashes com error_code)
                std::error_code ec;
                auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
                const auto end = fs::recursive_directory_iterator();

                while (it != end && !ec) {
                    if (it->is_regular_file(ec) && !ec) {
                        if (it->path().filename() == *search_query) {
                            engines::SearchResult res;
                            res.file_path   = fs::relative(it->path(), fs::current_path(), ec);
                            res.engine_name = "generic";

                            std::println("{}", formatter->format_search_results({res}));
                            break;
                        }
                    }
                    it.increment(ec);
                    if (ec) ec.clear(); // Limpa e ignora directórios corrompidos
                }
            }
        }

        // ==========================================
        // Módulo XREF (Especializado)
        // ==========================================
        auto run_xref = [&](const std::string& target, std::string direction, const std::vector<std::string>& opcodes = {}) {
            if (auto engine = scout::create_engine("xref")) {
                engines::SearchConfig xcfg;
                xcfg.query = target;
                xcfg.direction = direction;
                xcfg.filter_opcodes = opcodes;
                xcfg.include_system = config.xref_include_system;
                xcfg.search_depth = config.xref_depth;

                auto results = engine->search(analysis_ctx, xcfg);
                std::println("{}", formatter->format_xref_results(results));
            }
        };

        if (config.xref)         run_xref(*config.xref, config.xref_direction);
        if (config.xref_callers) run_xref(*config.xref_callers, "callers", {"invoke-"});
        if (config.xref_callees) run_xref(*config.xref_callees, "callees");
        if (config.xref_fields)  run_xref(*config.xref_fields, "callers", {"get", "put"});

        // ==========================================
        // Módulos Auxiliares de Análise
        // ==========================================

        if (config.cfg) {
            if (auto engine = scout::create_engine("cfg")) {
                engines::SearchConfig cfg;
                cfg.query = *config.cfg;
                std::println("{}", formatter->format_search_results(engine->search(analysis_ctx, cfg)));
            }
        }

        if (config.dump_ast) {
            if (auto engine = scout::create_engine("smali_dump")) {
                engines::SearchConfig dcfg;
                dcfg.query = *config.dump_ast;
                std::println("{}", formatter->format_search_results(engine->search(analysis_ctx, dcfg)));
            }
        }

        if (config.resource_map || config.find_resource) {
            if (auto engine = scout::create_engine("resource_map")) {
                engines::SearchConfig rcfg;
                rcfg.query = config.find_resource.value_or("");
                print_results(engine->search(analysis_ctx, rcfg));
            }
        }

        if (config.manifest) {
            if (auto engine = scout::create_engine("manifest")) {
                engines::SearchConfig mcfg;
                std::println("{}", formatter->format_search_results(engine->search(analysis_ctx, mcfg)));
            }
        }

        if (config.inspect_class) {
            if (auto engine = scout::create_engine("class_inspector")) {
                engines::SearchConfig icfg;
                icfg.query = *config.inspect_class;
                std::println("{}", formatter->format_search_results(engine->search(analysis_ctx, icfg)));
            }
        }

        if (config.ui_mapper) {
            if (auto engine = scout::create_engine("ui_mapper")) {
                engines::SearchConfig ucfg;
                ucfg.query = *config.ui_mapper;
                std::println("{}", formatter->format_search_results(engine->search(analysis_ctx, ucfg)));
            }
        }

        if (config.deobf_strings) {
            if (auto engine = scout::create_engine("deobf")) {
                engines::SearchConfig dcfg;
                std::println("{}", formatter->format_search_results(engine->search(analysis_ctx, dcfg)));
            }
        }

        if (config.detect_obfuscation) {
            if (auto engine = scout::create_engine("deobf")) {
                engines::SearchConfig dcfg;
                dcfg.query = search_query.value_or("");
                std::println("{}", formatter->format_search_results(engine->search(analysis_ctx, dcfg)));
            }
        }

        if (config.translate) {
            if (auto engine = scout::create_engine("smali_dump")) {
                engines::SearchConfig tcfg;
                tcfg.query = *config.translate;
                tcfg.search_type = "translate";
                std::println("{}", formatter->format_search_results(engine->search(analysis_ctx, tcfg)));
            }
        }

        if (config.track_var) {
            if (auto engine = scout::create_engine("taint_analysis")) {
                engines::SearchConfig vcfg;
                vcfg.query = *config.track_var;
                vcfg.var_name = config.track_var_name;
                vcfg.search_depth = config.track_depth;
                print_results(engine->search(analysis_ctx, vcfg));
            }
        }

        // ==========================================
        // Módulos Pendentes (Informação em SExpr)
        // ==========================================
        if (config.scan)   emit_pending("scan");
        if (config.hook)   emit_pending("hook");
        if (config.frida)  emit_pending("frida");

        // ==========================================
        // Output de Debugging/Verbose
        // ==========================================
        if (config.verbose) {
            // C++26: Geração de string combinada a partir de container, zero-allocation até a chamada final de ranges::to
            const auto engines = scout::list_available_engines();
            const std::string engines_str = engines
                                          | std::views::join_with(',')
                                          | std::ranges::to<std::string>();

            auto n = sexpr::form("scout:info");
            n.kv("dir",     sexpr::string(dir.string()));
            n.kv("engines", sexpr::string(engines_str));
            std::println("{}", formatter->format(n));
        }

    } catch (const std::exception& e) {
        // Redireciona a formatação de erro para a STDERR de forma Thread-Safe e rápida
        std::println(stderr, "{}", sexpr::make_error(e.what()).to_string());
        return 1;
    }

    return 0;
}
