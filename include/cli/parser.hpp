#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <functional>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace cli {

    class HelpRequested : public std::exception {
    public:
        const char* what() const noexcept override {
            return "Help requested";
        }
    };

    class ParseError : public std::invalid_argument {
    public:
        explicit ParseError(const std::string& msg) : std::invalid_argument(msg) {}
    };

    enum class ArgType { Flag, String, Int, StringList };

    struct Argument {
        std::string name;
        std::string metavar;
        std::string help;
        ArgType type;
        std::function<void(const std::vector<std::string_view>&)> parse_func;
    };

    class Parser {
        std::vector<Argument> args_;
        std::string description_;
        std::string epilog_;
        std::function<void(std::string_view)> positional_func_;

        static bool is_flag(std::string_view s) {
            if (s.size() < 2) return false;
            if (s[0] == '-' && std::isalpha(static_cast<unsigned char>(s[1]))) return true;
            if (s.size() >= 3 && s[0] == '-' && s[1] == '-' && std::isalpha(static_cast<unsigned char>(s[2]))) return true;
            return false;
        }

    public:
        Parser(std::string desc, std::string epilog)
            : description_(std::move(desc)), epilog_(std::move(epilog)) {}

        void set_positional(std::function<void(std::string_view)> func) {
            positional_func_ = std::move(func);
        }

        void add_flag(std::string name, std::string help, bool& bind) {
            args_.push_back({std::move(name), "", std::move(help), ArgType::Flag,
                [&bind](const std::vector<std::string_view>&) { bind = true; }});
        }

        void add_option(std::string name, std::string metavar, std::string help, std::optional<std::string>& bind) {
            args_.push_back({std::move(name), std::move(metavar), std::move(help), ArgType::String,
                [&bind, name](const std::vector<std::string_view>& vals) {
                    if (vals.empty()) throw ParseError("Faltando valor para " + name);
                    bind = std::string(vals[0]);
                }});
        }
        
        void add_option(std::string name, std::string metavar, std::string help, std::string& bind) {
            args_.push_back({std::move(name), std::move(metavar), std::move(help), ArgType::String,
                [&bind, name](const std::vector<std::string_view>& vals) {
                    if (vals.empty()) throw ParseError("Faltando valor para " + name);
                    bind = std::string(vals[0]);
                }});
        }

        void add_option(std::string name, std::string metavar, std::string help, int& bind) {
            args_.push_back({std::move(name), std::move(metavar), std::move(help), ArgType::Int,
                [&bind, name](const std::vector<std::string_view>& vals) {
                    if (vals.empty()) throw ParseError("Faltando valor para " + name);
                    try {
                        bind = std::stoi(std::string(vals[0]));
                    } catch (...) {
                        throw ParseError("Valor inteiro inválido para " + name + ": " + std::string(vals[0]));
                    }
                }});
        }

        void add_list(std::string name, std::string metavar, std::string help, std::vector<std::string>& bind) {
            args_.push_back({std::move(name), std::move(metavar), std::move(help), ArgType::StringList,
                [&bind, name](const std::vector<std::string_view>& vals) {
                    if (vals.empty()) throw ParseError("Faltando valores para " + name);
                    bind.clear();
                    for (auto v : vals) bind.emplace_back(v);
                }});
        }

        void print_help(std::ostream& os, const char* prog_name) const {
            os << description_ << "\n\n";
            os << "Uso: " << prog_name << " [OPCOES]\n\n";
            os << "Opções:\n";
            os << "  " << std::left << std::setw(38) << "--help" << "Mostra esta mensagem de ajuda e sai.\n";
            
            for (const auto& arg : args_) {
                std::string flag = arg.name;
                if (!arg.metavar.empty()) {
                    if (arg.type == ArgType::StringList) {
                        flag += " " + arg.metavar + " [" + arg.metavar + " ...]";
                    } else {
                        flag += " " + arg.metavar;
                    }
                }
                os << "  " << std::left << std::setw(38) << flag << arg.help << "\n";
            }
            os << "\n" << epilog_ << "\n";
        }

        void parse(int argc, const char* const* argv) {
            std::vector<std::string_view> tokens;
            for (int i = 1; i < argc; ++i) {
                if (argv[i] != nullptr) {
                    tokens.push_back(argv[i]);
                }
            }

            if (tokens.empty()) {
                throw HelpRequested();
            }

            for (size_t i = 0; i < tokens.size(); ++i) {
                if (tokens[i] == "--help" || tokens[i] == "-h") {
                    throw HelpRequested();
                }

                if (!is_flag(tokens[i])) {
                    if (positional_func_) {
                        positional_func_(tokens[i]);
                        continue;
                    } else {
                        throw ParseError("Argumento posicional inesperado: " + std::string(tokens[i]));
                    }
                }

                auto it = std::find_if(args_.begin(), args_.end(), [&](const Argument& a) {
                    return a.name == tokens[i];
                });

                if (it == args_.end()) {
                    throw ParseError("Argumento desconhecido: " + std::string(tokens[i]));
                }

                if (it->type == ArgType::Flag) {
                    it->parse_func({});
                } else if (it->type == ArgType::StringList) {
                    std::vector<std::string_view> vals;
                    while (i + 1 < tokens.size() && !is_flag(tokens[i + 1])) {
                        vals.push_back(tokens[++i]);
                    }
                    it->parse_func(vals);
                } else { // String or Int
                    if (i + 1 >= tokens.size() || is_flag(tokens[i + 1])) {
                        throw ParseError("Faltando valor para " + std::string(tokens[i]));
                    }
                    it->parse_func({tokens[++i]});
                }
            }
        }
    };

    // ==========================================
    // Estrutura que armazena a configuração CLI
    // ==========================================
    struct ScoutConfig {
        bool manifest = false;
        std::optional<std::string> scan;
        bool resource_map = false;
        std::optional<std::string> find_resource;
        std::optional<std::string> search;
        bool machine_sexpr = false;
        std::string output_format = "sexpr";
        std::optional<std::string> path;
        std::string progress = "basic";
        bool dry_run = false;
        std::optional<std::string> batch;
        std::string search_type = "regex";
        std::optional<std::string> search_in;
        std::optional<std::string> search_exclude;
        int search_max = 1000;
        std::optional<std::string> brain;
        std::optional<std::string> xref;
        std::string xref_direction = "both";
        int xref_depth = 1;
        bool xref_include_system = false;
        std::optional<std::string> hook;
        std::optional<std::string> frida;
        std::optional<std::string> cfg;
        std::optional<std::string> ui_trace;
        bool reason = false;
        std::optional<std::string> translate;
        std::optional<std::string> track_var;
        std::vector<std::string> list_methods;
        std::string track_var_name = "p2";
        int track_depth = 10;
        std::optional<std::string> track_format;
        bool detect_obfuscation = false;
        std::optional<std::string> code_metrics;
        std::vector<std::string> obf_types = {"all"};
        int obf_depth = 3;
        std::optional<std::string> analyze_data_flow;
        int data_flow_depth = 2;
        bool export_report = false;
        bool introspect_sexpr = false;
        bool generate_hook_class = false;
        std::vector<std::string> patch_manifest;
        std::optional<std::string> scan_rules;
        std::optional<std::string> graph;
        bool ai_help = false;
        bool verbose = false;
        std::vector<std::string> positional_args;

        static std::optional<ScoutConfig> parse(int argc, const char* const* argv) {
            ScoutConfig config;

            std::string desc = "Scout\nFerramenta para análise estática, recon, patching e instrumentação de projetos Android em smali.";
            std::string epilog = R"(Exemplos rápidos:
  Recon geral:
    scout --manifest --scan all

  Investigar uma classe:
    scout --brain Lcom/example/AuthManager;

  Gerar hook Frida:
    scout --frida 'Lcom/example/Net;->send([BLjava/lang/String;)Z'

  Aplicar patch estático:
    scout --hook 'Lcom/example/LoginActivity;->doLogin(Ljava/lang/String;Ljava/lang/String;)V'

  Buscar IDs de recursos e suas definições:
    scout --scan integers

Ajuda expandida para IA:
  scout --ai-help

Introspecção em S-Expression:
  scout --introspect-sexpr)";

            cli::Parser parser(desc, epilog);

            parser.set_positional([&config](std::string_view val) {
                config.positional_args.push_back(std::string(val));
            });

            // Registrando todos os argumentos mapeados a partir do Python
            parser.add_flag("--manifest", "Analisa o AndroidManifest.xml e extrai flags do app e componentes.", config.manifest);
            parser.add_option("--scan", "{vuln,crypto,strings,integers,all}", "Executa scanners estáticos.", config.scan);
            parser.add_flag("--resource-map", "Mostra o mapeamento de resource IDs encontrados (requer --scan integers).", config.resource_map);
            parser.add_option("--find-resource", "RESOURCE_ID", "Encontra uso de um resource ID (ex: 0x7f0b0000).", config.find_resource);
            parser.add_option("--search", "QUERY", "Busca genérica no código Smali.", config.search);
            parser.add_flag("--machine-sexpr", "Output apenas em S-Expression (sem logs humanos).", config.machine_sexpr);
            parser.add_option("--output-format", "{sexpr,json,text,yaml}", "Formato de saída (padrão: sexpr).", config.output_format);
            parser.add_option("--path", "DIRECTORY", "Caminho do APK descompilado (padrão: dir atual).", config.path);
            parser.add_option("--progress", "{none,basic,detailed}", "Relatório de progresso (padrão: basic).", config.progress);
            parser.add_flag("--dry-run", "Mostra o que seria feito sem aplicar as mudanças (para --hook).", config.dry_run);
            parser.add_option("--batch", "FILE", "Executa múltiplos comandos a partir de um arquivo.", config.batch);
            parser.add_option("--search-type", "TYPE", "Tipo de busca para --search (padrão: regex).", config.search_type);
            parser.add_option("--search-in", "DIRS", "Diretórios específicos para buscar (ex: smali,smali_classes2).", config.search_in);
            parser.add_option("--search-exclude", "DIRS", "Diretórios para excluir da busca.", config.search_exclude);
            parser.add_option("--search-max", "INT", "Número máximo de resultados (padrão: 1000).", config.search_max);
            parser.add_option("--brain", "CLASS", "Analisa uma classe smali e lista as APIs mais frequentes.", config.brain);
            parser.add_option("--xref", "TARGET", "Análise de cross-reference: quem chama ou o que é chamado.", config.xref);
            parser.add_option("--xref-direction", "{callers,callees,both}", "Direção para --xref (padrão: both).", config.xref_direction);
            parser.add_option("--xref-depth", "INT", "Profundidade máxima para XREF recursivo (padrão: 1).", config.xref_depth);
            parser.add_flag("--xref-include-system", "Inclui classes do sistema Android no XREF.", config.xref_include_system);
            parser.add_option("--hook", "METHOD_SIGNATURE", "Aplica patch injetando invoke-static no início do método.", config.hook);
            parser.add_option("--frida", "METHOD_SIGNATURE", "Gera script Frida para um método específico.", config.frida);
            parser.add_option("--cfg", "METHOD_SIGNATURE", "Gera um grafo de fluxo de controle (DOT).", config.cfg);
            parser.add_option("--ui-trace", "ID_OR_NAME", "Rastreia classe Smali que lida com UI (ex: btn_login).", config.ui_trace);
            parser.add_flag("--reason", "Faz uma síntese lógica das descobertas.", config.reason);
            parser.add_option("--translate", "METHOD_SIGNATURE", "Traduz um método Smali para pseudocódigo.", config.translate);
            parser.add_option("--track-var", "METHOD_SIGNATURE", "Rastreia o fluxo de uma variável.", config.track_var);
            parser.add_list("--list-methods", "CLASS", "Lista os métodos contidos nas classes especificadas.", config.list_methods);
            parser.add_option("--track-var-name", "VARIABLE", "Nome da variável a rastrear (padrão: p2).", config.track_var_name);
            parser.add_option("--track-depth", "DEPTH", "Profundidade para rastreamento (padrão: 10).", config.track_depth);
            parser.add_option("--track-format", "FORMAT", "Formato de output (json, dot, mermaid, both).", config.track_format);
            parser.add_flag("--detect-obfuscation", "Detecta técnicas de obfuscação.", config.detect_obfuscation);
            parser.add_option("--code-metrics", "CLASS_SIGNATURE", "Gera métricas de código.", config.code_metrics);
            parser.add_list("--obf-types", "TYPE", "Tipos de detecção de obfuscação (padrão: all).", config.obf_types);
            parser.add_option("--obf-depth", "DEPTH", "Profundidade para tracking dinâmico (padrão: 3).", config.obf_depth);
            parser.add_option("--analyze-data-flow", "CLASS_SIGNATURE", "Analisa fluxos de dados sensíveis.", config.analyze_data_flow);
            parser.add_option("--data-flow-depth", "DEPTH", "Profundidade para data flow (padrão: 2).", config.data_flow_depth);
            parser.add_flag("--export", "Força a exportação do scout_report.json.", config.export_report);
            parser.add_flag("--introspect-sexpr", "Imprime capacidades em S-Expression e encerra.", config.introspect_sexpr);
            parser.add_flag("--generate-hook-class", "Gera a classe smali ScoutHook.", config.generate_hook_class);
            parser.add_list("--patch-manifest", "KEY=VALUE", "Modifica AndroidManifest.xml.", config.patch_manifest);
            parser.add_option("--scan-rules", "RULES_JSON", "Executa regras de scanner personalizadas.", config.scan_rules);
            parser.add_option("--graph", "OUTPUT_FILE", "Gera um arquivo DOT com dependências.", config.graph);
            parser.add_flag("--ai-help", "Mostra a documentação orientada para IA e encerra.", config.ai_help);
            parser.add_flag("--verbose", "Ativa logs detalhados.", config.verbose);

            try {
                parser.parse(argc, argv);
                return config;
            } catch (const HelpRequested&) {
                parser.print_help(std::cout, argc > 0 && argv && argv[0] ? argv[0] : "scout");
                return std::nullopt;
            }
        }
    };
} // namespace cli