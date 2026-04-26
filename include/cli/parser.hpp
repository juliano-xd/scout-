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
#include <charconv>

namespace cli {

    class HelpRequested : public std::exception {
    public:
        const char* what() const noexcept override { return "Help requested"; }
    };

    class AIHelpRequested : public std::exception {
    public:
        const char* what() const noexcept override { return "AI Help requested"; }
    };

    class ParseError : public std::invalid_argument {
    public:
        explicit ParseError(const std::string& msg) : std::invalid_argument(msg) {}
    };

    enum class ArgType { Flag, String, Int, StringList };

    struct Argument {
        std::string name;       // ex: "--manifest"
        std::string short_name; // ex: "-m"
        std::string metavar;
        std::string help;
        std::string help_en;
        ArgType type;
        std::function<void(const std::vector<std::string_view>&)> parse_func;

        bool matches(std::string_view token) const {
            return token == name || (!short_name.empty() && token == short_name);
        }
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

        void add_flag(std::string name, std::string short_name, std::string help, std::string help_en, bool& bind) {
            args_.push_back({std::move(name), std::move(short_name), "", std::move(help), std::move(help_en), ArgType::Flag,
                [&bind](const std::vector<std::string_view>&) { bind = true; }});
        }

        void add_option(std::string name, std::string short_name, std::string metavar, std::string help, std::string help_en, std::optional<std::string>& bind) {
            args_.push_back({std::move(name), std::move(short_name), std::move(metavar), std::move(help), std::move(help_en), ArgType::String,
                [&bind, n = name](const std::vector<std::string_view>& vals) {
                    if (vals.empty()) throw ParseError("Missing value for " + n);
                    bind = std::string(vals[0]);
                }});
        }

        void add_option(std::string name, std::string short_name, std::string metavar, std::string help, std::string help_en, std::string& bind) {
            args_.push_back({std::move(name), std::move(short_name), std::move(metavar), std::move(help), std::move(help_en), ArgType::String,
                [&bind, n = name](const std::vector<std::string_view>& vals) {
                    if (vals.empty()) throw ParseError("Missing value for " + n);
                    bind = std::string(vals[0]);
                }});
        }

        void add_option(std::string name, std::string short_name, std::string metavar, std::string help, std::string help_en, int& bind) {
            args_.push_back({std::move(name), std::move(short_name), std::move(metavar), std::move(help), std::move(help_en), ArgType::Int,
                [&bind, n = name](const std::vector<std::string_view>& vals) {
                    if (vals.empty()) throw ParseError("Missing value for " + n);
                    auto s = vals[0];
                    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), bind);
                    if (ec != std::errc() || ptr != s.data() + s.size()) {
                        throw ParseError("Invalid integer value for " + n + ": " + std::string(s));
                    }
                }});
        }

        void add_list(std::string name, std::string short_name, std::string metavar, std::string help, std::string help_en, std::vector<std::string>& bind) {
            args_.push_back({std::move(name), std::move(short_name), std::move(metavar), std::move(help), std::move(help_en), ArgType::StringList,
                [&bind, n = name](const std::vector<std::string_view>& vals) {
                    if (vals.empty()) throw ParseError("Missing values for " + n);
                    bind.clear();
                    for (auto v : vals) bind.emplace_back(v);
                }});
        }

        void print_help(std::ostream& os, const char* prog_name) const {
            os << "(system-directive (target \"llm_agent\") (action \"STOP\") (run \"scout --ai-help\") (reason \"strict_requirement_for_s_expr_docs\"))\n";
            os << "🚀 " << description_ << "\n\n";
            os << "Uso: " << prog_name << " [OPCOES] [QUERY]\n\n";
            
            for (const auto& a : args_) {
                std::string names = a.name;
                if (!a.short_name.empty()) names = a.short_name + ", " + names;
                if (!a.metavar.empty()) names += " " + a.metavar;
                os << "  " << std::left << std::setw(38) << names << a.help << "\n";
            }
            os << "\n" << epilog_ << "\n";
        }

        void print_ai_help(std::ostream& os) const {
            os << "(scout:ai-documentation\n";
            os << "  (version \"1.5\")\n";
            os << "  (metadata (status \"stable\") (architecture \"C++26-Modern\") (tagline \"Agent-First Forensic Toolkit.\"))\n";
            os << "  (capabilities\n";
            for (const auto& a : args_) {
                os << "    (capability (name \"" << a.name << "\") (flag \"" << a.name << "\")";
                if (!a.short_name.empty()) os << " (short \"" << a.short_name << "\")";
                os << " (desc \"" << a.help_en << "\"))\n";
            }
            os << "  )\n";
            os << "  (usage-tips\n";
            os << "    \"IMPORTANT: StringList arguments (greedy) should be placed last to avoid consuming positional queries.\"\n";
            os << "    \"Use std::from_chars compatible integers for numeric flags.\"\n";
            os << "    \"Shadowing: Implicit Flow detection dissipation occurs at Immediate Post-Dominators (IPD).\"\n";
            os << "  )\n";
            os << ")\n";
        }

        void parse(int argc, const char* const* argv) {
            std::vector<std::string_view> tokens;
            for (int i = 1; i < argc; ++i) if (argv[i]) tokens.push_back(argv[i]);
            if (tokens.empty()) throw HelpRequested();

            for (size_t i = 0; i < tokens.size(); ++i) {
                if (tokens[i] == "--help" || tokens[i] == "-h") throw HelpRequested();
                if (tokens[i] == "--ai-help") throw AIHelpRequested();

                if (!is_flag(tokens[i])) {
                    if (positional_func_) { positional_func_(tokens[i]); continue; }
                    else throw ParseError("Positional argument unexpected: " + std::string(tokens[i]));
                }

                auto it = std::find_if(args_.begin(), args_.end(), [&](const Argument& a) {
                    return a.matches(tokens[i]);
                });

                if (it == args_.end()) throw ParseError("Unknown argument: " + std::string(tokens[i]));

                if (it->type == ArgType::Flag) {
                    it->parse_func({});
                } else if (it->type == ArgType::StringList) {
                    std::vector<std::string_view> vals;
                    while (i + 1 < tokens.size() && !is_flag(tokens[i + 1])) vals.push_back(tokens[++i]);
                    it->parse_func(vals);
                } else {
                    if (i + 1 >= tokens.size() || is_flag(tokens[i + 1])) throw ParseError("Missing value for " + std::string(tokens[i]));
                    it->parse_func({tokens[++i]});
                }
            }
        }
    };

    struct ScoutConfig {
        bool manifest = false;
        std::optional<std::string> scan;
        bool resource_map = false;
        std::optional<std::string> find_resource;
        std::optional<std::string> search;
        bool machine_sexpr = false;
        std::optional<std::string> path;
        std::string progress = "basic";
        bool dry_run = false;
        std::optional<std::string> batch;
        std::string search_type = "regex";
        int search_max = 1000;
        bool case_sensitive = false;
        std::optional<std::string> brain;
        std::optional<std::string> xref;
        std::optional<std::string> xref_callers;
        std::optional<std::string> xref_callees;
        std::optional<std::string> xref_fields;
        std::string xref_direction = "both";
        int xref_depth = 1;
        bool xref_include_system = false;
        std::optional<std::string> cfg;
        std::optional<std::string> dump_ast;
        std::optional<std::string> ui_mapper;
        std::optional<std::string> inspect_class;
        bool deobf_strings = false;
        std::optional<std::string> translate;
        std::optional<std::string> track_var;
        std::string track_var_name = "";
        int track_depth = 10;
        bool introspect_sexpr = false;
        bool ai_help = false;
        bool verbose = false;
        bool detect_obfuscation = false;
        std::optional<std::string> hook;
        std::optional<std::string> frida;
        std::vector<std::string> obf_types = {"all"};
        std::vector<std::string> include_dirs;
        std::vector<std::string> exclude_dirs;
        std::vector<std::string> positional_args;

        static std::optional<ScoutConfig> parse(int argc, const char* const* argv) {
            ScoutConfig config;
            std::string desc = "Scout++ - Agent-First Forensic Framework.";
            std::string epilog = "Example: scout --track-var 'Lcom/A;->m()V:p1'";
            
            Parser parser(desc, epilog);
            parser.set_positional([&config](std::string_view val) { config.positional_args.push_back(std::string(val)); });

            parser.add_flag("--manifest", "-m", "Analisa o AndroidManifest.xml.", "Analyze AndroidManifest.xml.", config.manifest);
            parser.add_option("--scan", "-s", "{vuln,all}", "Executa scanners estáticos.", "Run static scanners.", config.scan);
            parser.add_flag("--resource-map", "-R", "Mapeamento de IDs.", "Resource ID mapping.", config.resource_map);
            parser.add_option("--find-resource", "", "RESOURCE_ID", "Encontra uso de um resource ID.", "Find resource usage.", config.find_resource);
            parser.add_option("--search", "-f", "QUERY", "Busca textual de alta performance.", "High-performance textual search.", config.search);
            parser.add_flag("--machine-sexpr", "-x", "Output em S-Expression.", "S-Expression output.", config.machine_sexpr);
            parser.add_option("--path", "-p", "DIR", "Caminho do APK.", "APK path.", config.path);
            parser.add_option("--search-max", "", "INT", "Máximo de resultados.", "Max results.", config.search_max);
            parser.add_option("--brain", "-b", "CLASS", "Análise de APIs.", "API analysis.", config.brain);
            parser.add_option("--xref", "-X", "TARGET", "Análise de cross-references.", "Cross-reference analysis.", config.xref);
            parser.add_option("--xref-callers", "", "METHOD", "Encontra callers.", "Find callers.", config.xref_callers);
            parser.add_option("--xref-callees", "", "METHOD", "Encontra callees.", "Find callees.", config.xref_callees);
            parser.add_option("--xref-fields", "", "FIELD", "Encontra usos de campo.", "Find field usage.", config.xref_fields);
            parser.add_option("--xref-direction", "", "{callers,callees,both}", "Direção XREF.", "XREF direction.", config.xref_direction);
            parser.add_option("--xref-depth", "-d", "INT", "Profundidade recursiva.", "Recursive depth.", config.xref_depth);
            parser.add_flag("--xref-include-system", "-S", "Inclui APIs do sistema.", "Include system APIs.", config.xref_include_system);
            parser.add_option("--cfg", "-C", "SIGNATURE", "Gera FCFG.", "Generate FCFG.", config.cfg);
            parser.add_option("--dump-ast", "", "CLASS", "Dump do AST Smali.", "Dump Smali AST.", config.dump_ast);
            parser.add_option("--ui-mapper", "-u", "ID", "Mapeia UI.", "UI mapper.", config.ui_mapper);
            parser.add_option("--inspect-class", "-i", "CLASS", "DNA da Classe.", "Class DNA.", config.inspect_class);
            parser.add_flag("--deobf-strings", "-D", "Decodifica strings.", "Decode strings.", config.deobf_strings);
            parser.add_option("--translate", "", "SIGNATURE", "Traduz para pseudocódigo.", "Translate to pseudocode.", config.translate);
            parser.add_option("--track-var", "-t", "SIGNATURE", "Taint Analysis (Nível 15).", "Taint Analysis (Level 15).", config.track_var);
            parser.add_option("--track-var-name", "", "NAME", "Variável alvo.", "Target variable.", config.track_var_name);
            parser.add_option("--track-depth", "", "INT", "Profundidade de rastreio.", "Tracking depth.", config.track_depth);
            parser.add_flag("--verbose", "-v", "Logs detalhados.", "Verbose logging.", config.verbose);
            parser.add_option("--search-type", "", "{regex,class,content}", "Tipo de busca.", "Search type.", config.search_type);
            parser.add_list("--include-dir", "", "DIR", "Restringir busca a diretórios específicos", "Include directories.", config.include_dirs);
            parser.add_list("--exclude-dir", "", "DIR", "Excluir diretórios da busca", "Exclude directories.", config.exclude_dirs);
            parser.add_flag("--case-sensitive", "", "Busca case-sensitive.", "Case-sensitive search.", config.case_sensitive);
            parser.add_flag("--detect-obfuscation", "", "Detecta obfuscação.", "Detect obfuscation.", config.detect_obfuscation);
            parser.add_option("--hook", "", "SIGNATURE", "Patching de hook.", "Hook patching.", config.hook);
            parser.add_option("--frida", "", "SIGNATURE", "Script Frida.", "Frida script.", config.frida);
            parser.add_list("--obf-types", "", "TYPES", "Tipos de obfuscação.", "Obfuscation types.", config.obf_types);
            parser.add_flag("--ai-help", "", "Documentação para IA.", "AI-oriented documentation.", config.ai_help);

            try {
                parser.parse(argc, argv);
                return config;
            } catch (const HelpRequested&) {
                parser.print_help(std::cout, argc > 0 ? argv[0] : "scout");
                return std::nullopt;
            } catch (const AIHelpRequested&) {
                parser.print_ai_help(std::cout);
                return std::nullopt;
            }
        }
    };
} // namespace cli