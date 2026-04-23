#include "cli/parser.hpp"
#include "core/scanner.hpp"
#include "core/smali_parser.hpp"
#include "analysis/xref.hpp"
#include "utils/filesystem.hpp"
#include "utils/sexpr.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <string_view>
#include <optional>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

int main(int argc, char** argv) {
    json response;
    try {
        // Parse da linha de comando
        auto config_opt = cli::ScoutConfig::parse(argc, argv);
        if (!config_opt) {
            return 0; // Ajuda ou parâmetros vazios
        }
        
        const auto& config = *config_opt;
        fs::path dir = config.path.value_or(fs::current_path().string());
        
        response["status"] = "success";
        
        // Logs de Verbose e Status Base
        if (config.verbose) {
            response["logs"].push_back("[VERBOSE] Modo verboso ativado.");
        }
        if (config.manifest) {
            response["logs"].push_back("[INFO] Analisando o AndroidManifest.xml...");
        }
        if (config.scan) {
            response["logs"].push_back("[INFO] Iniciando scanner: " + *config.scan);
        }

        // ==========================================
        // Módulo de Busca (Classes, Conteúdo Interno e Arquivos Genéricos)
        // ==========================================
        std::optional<std::string> search_query = config.search;
        if (!search_query && !config.positional_args.empty()) {
            search_query = config.positional_args[0];
        }

        if (search_query) {
            json search_results = json::array();
            
            // Se o usuário pedir explicitamente por classe, ou se parecer uma notação de classe Dalvik
            if (config.search_type == "class" || (search_query->find('L') == 0 && search_query->back() == ';')) {
                if (search_query->find('L') == 0 && search_query->back() == ';') {
                    auto file = core::find_class_file(dir, *search_query);
                    if (file) {
                        search_results.push_back(fs::relative(*file, fs::current_path()).string());
                    }
                } else {
                    auto files = core::find_classes_containing(dir, *search_query);
                    for (const auto& f : files) {
                        search_results.push_back(fs::relative(f, fs::current_path()).string());
                    }
                }
            } 
            // Busca por conteúdo interno (strings, regex)
            else if (config.search_type == "regex" || config.search_type == "string" || config.search_type == "number" || config.search_type == "integer") {
                auto matches = core::search_content(dir, *search_query, config.search_type);
                for (const auto& match : matches) {
                    search_results.push_back({
                        {"file", fs::relative(match.file_path, fs::current_path()).string()},
                        {"line", match.line_number},
                        {"content", match.line_content}
                    });
                }
            } 
            // Fallback genérico para arquivos
            else {
                auto file = utils::buscar_arquivo_recursivo(dir, *search_query);
                if (file) {
                    search_results.push_back(fs::relative(*file, fs::current_path()).string());
                }
            }
            
            response["search"] = {
                {"query", *search_query},
                {"type", config.search_type},
                {"results", search_results}
            };
        }

        // ==========================================
        // Módulo de Listagem de Métodos em Múltiplas Classes
        // ==========================================
        if (!config.list_methods.empty()) {
            json methods_results = json::array();
            for (const auto& class_name : config.list_methods) {
                auto file = core::find_class_file(dir, class_name);
                json class_info;
                class_info["class_name"] = class_name;
                
                if (file) {
                    class_info["file_path"] = fs::relative(*file, fs::current_path()).string();
                    auto methods = core::SmaliParser::extract_methods(*file);
                    
                    json methods_json = json::array();
                    for (const auto& m : methods) {
                        methods_json.push_back({
                            {"modifiers", m.access_modifiers},
                            {"name", m.name},
                            {"signature", m.signature}
                        });
                    }
                    
                    class_info["methods"] = methods_json;
                    class_info["found"] = true;
                } else {
                    class_info["found"] = false;
                    class_info["error"] = "Class not found in target directory.";
                }
                methods_results.push_back(class_info);
            }
            response["list_methods"] = methods_results;
        }

        // ==========================================
        // Módulo de Cross-Reference (XREF)
        // ==========================================
        if (config.xref) {
            json xref_results = json::array();
            auto callers = analysis::XrefEngine::find_callers(dir, *config.xref);
            
            for (const auto& caller : callers) {
                xref_results.push_back({
                    {"file", fs::relative(caller.caller_file, fs::current_path()).string()},
                    {"class", caller.caller_class},
                    {"method", caller.caller_method},
                    {"line", caller.line_number},
                    {"instruction", caller.instruction}
                });
            }
            
            response["xref"] = {
                {"target", *config.xref},
                {"total_references", callers.size()},
                {"callers", xref_results}
            };
        }

        // O output primário para o Agente de IA é S-Expression (a não ser que JSON seja exigido)
        if (config.output_format == "json") {
            std::cout << response.dump(4) << "\n";
        } else {
            std::cout << utils::json_to_sexpr(response) << "\n";
        }

    } catch (const std::exception& e) {
        response["status"] = "error";
        response["error_message"] = e.what();
        std::cerr << utils::json_to_sexpr(response) << "\n";
        return 1;
    }

    return 0;
}
