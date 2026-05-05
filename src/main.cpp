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
        // Parse de argumentos lock-free e zero-allocation
        auto config_opt = cli::ScoutConfig::parse(argc, argv);
        if (!config_opt) return 0;

        const auto& config = *config_opt;

        // OPTIMIZAÇÃO: Evita alocar current_path().string() na Heap se config.path já existir
        const fs::path dir = config.path ? fs::path(*config.path) : fs::current_path();

        // Instancia o contexto central (Carrega a AST do Smali em memória)
        core::AnalysisContext analysis_ctx(dir);

        // Registrar motores e formatador
        scout::register_all_components();

        // Obter formatador (sempre sexpr na base do motor)
        auto formatter = scout::create_formatter("sexpr");
        if (!formatter) {
            std::println(stderr, "{}", sexpr::make_error("Falha ao inicializar formatador sexpr").to_string());
            return 1;
        }

        // ==========================================
        // Infraestrutura de Despacho (Dispatchers)
        // ==========================================

        // Lambda de formatação unificada (Garante que a flag --machine-sexpr é RESPEITADA por todos)
        auto print_results = [&](const std::vector<engines::SearchResult>& results) {
            if (config.machine_sexpr) {
                const auto args = sexpr::list({sexpr::keyword("pretty"), sexpr::boolean(true)});
                std::println("{}", formatter->format_search_results(results, args));
            } else {
                std::println("{}", formatter->format_search_results(results));
            }
        };

        // Lambda de Alta-Ordem (HOC) para executar qualquer motor com segurança.
        // C++20: Utiliza Template Param explícito para providenciar um tipo de fallback
        // (ponteiro de função) quando o modifier não é providenciado pelo chamador,
        // mitigando o erro de "dedução falhada no auto" sem recorrer ao lento std::function.
        auto run_standard_module = [&]<typename Func = void(*)(engines::SearchConfig&)>(
            std::string_view engine_name,
            std::string_view query = "",
            Func config_modifier = [](engines::SearchConfig&){}
        ) {
            if (auto engine = scout::create_engine(std::string(engine_name))) {
                engines::SearchConfig scfg;

                // Herança Global de Contexto (Bugfix crítico das versões anteriores)
                scfg.query        = query;
                scfg.max_results  = static_cast<std::size_t>(config.search_max);
                scfg.include_dirs = config.include_dirs;
                scfg.exclude_dirs = config.exclude_dirs;

                // Aplica modificadores específicos do motor invocado
                config_modifier(scfg);

                auto results = engine->search(analysis_ctx, scfg);
                print_results(results);
                return true;
            }
            return false;
        };

        auto emit_pending = [&](std::string_view module) {
            auto n = sexpr::form("scout:pending");
            n.kv("module",  sexpr::string(module));
            n.kv("status",  sexpr::string("not_implemented"));
            std::println("{}", formatter->format(n));
        };

        // ==========================================
        // Módulo de Busca Primária (Classes / Conteúdo)
        // ==========================================
        std::optional<std::string> search_query = config.search;
        if (!search_query && !config.positional_args.empty()) {
            search_query = config.positional_args[0];
        }

        if (search_query) {
            const std::string engine_name = scout::map_search_type_to_engine(config.search_type, *search_query);

            const bool engine_ran = run_standard_module(engine_name, *search_query, [&](auto& scfg) {
                scfg.search_type    = config.search_type;
                scfg.case_sensitive = config.case_sensitive;
            });

            if (!engine_ran) {
                // Fallback: Busca genérica de sistema de ficheiros com iterador seguro (No-Throw)
                std::error_code ec;
                auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
                const auto end = fs::recursive_directory_iterator();

                while (it != end && !ec) {
                    if (it->is_regular_file(ec) && !ec) {
                        if (it->path().filename() == *search_query) {
                            engines::SearchResult res;
                            res.file_path   = fs::relative(it->path(), fs::current_path(), ec);
                            res.engine_name = "generic";

                            print_results({res}); // Unificado
                            break;
                        }
                    }
                    it.increment(ec);
                    if (ec) ec.clear(); // Ignora e salta directórios corrompidos
                }
            }
        }

        // ==========================================
        // Módulo XREF (Especializado)
        // ==========================================
        auto run_xref = [&](const std::string& target, std::string_view direction, const std::vector<std::string>& opcodes = {}) {
            run_standard_module("xref", target, [&](auto& xcfg) {
                xcfg.direction      = std::string(direction);
                xcfg.filter_opcodes = opcodes;
                xcfg.include_system = config.xref_include_system;
                xcfg.search_depth   = config.xref_depth;
            });
        };

        if (config.xref)         run_xref(*config.xref, config.xref_direction);
        if (config.xref_callers) run_xref(*config.xref_callers, "callers", {"invoke-"});
        if (config.xref_callees) run_xref(*config.xref_callees, "callees");
        if (config.xref_fields)  run_xref(*config.xref_fields, "callers", {"get", "put"});

        // ==========================================
        // Módulos Auxiliares de Análise Dinâmica / Estática
        // ==========================================

        if (config.cfg) {
            run_standard_module("cfg", *config.cfg);
        }

        if (config.dump_ast) {
            run_standard_module("smali_dump", *config.dump_ast);
        }

        if (config.resource_map || config.find_resource) {
            run_standard_module("resource_map", config.find_resource.value_or(""));
        }

        if (config.manifest) {
            run_standard_module("manifest");
        }

        if (config.inspect_class) {
            run_standard_module("class_inspector", *config.inspect_class);
        }

        if (config.ui_mapper) {
            run_standard_module("ui_mapper", *config.ui_mapper);
        }

        if (config.deobf_strings) {
            run_standard_module("deobf");
        }

        if (config.detect_obfuscation) {
            run_standard_module("deobf", search_query.value_or(""));
        }

        if (config.translate) {
            run_standard_module("smali_dump", *config.translate, [](auto& tcfg) {
                tcfg.search_type = "translate";
            });
        }

        if (config.track_var) {
            run_standard_module("taint_analysis", *config.track_var, [&](auto& vcfg) {
                vcfg.var_name     = config.track_var_name;
                vcfg.search_depth = config.track_depth;
            });
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
            // C++26: Pipeline de Ranges puro. Zero-allocation loop, converte diretamente no final.
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
        // Redireciona a formatação de erro para a STDERR garantindo que logs de erro não sujem o STDOUT (pipes de IA)
        std::println(stderr, "{}", sexpr::make_error(e.what()).to_string());
        return 1;
    }

    return 0;
}
