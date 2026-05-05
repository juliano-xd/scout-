#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <fstream>
#include <execution>
#include <mutex>

namespace analysis {

    struct XrefResult {
        std::filesystem::path caller_file;
        std::string caller_class;
        std::string caller_method;
        size_t line_number;
        std::string instruction;
    };

    class XrefEngine {
    public:
        /**
         * @brief Encontra todos os lugares ("callers") onde um alvo ("callee") é chamado ou referenciado.
         * @param search_dir O diretório raiz (ex: wpp_decompiled)
         * @param target A classe (ex: Lcom/ex/A;) ou método (Lcom/ex/A;->met()V) a ser buscado
         * @return Vetor estruturado com os ponteiros exatos.
         */
        static std::vector<XrefResult> find_callers(const std::filesystem::path& search_dir, std::string_view target) {
            std::vector<XrefResult> results;
            if (!std::filesystem::exists(search_dir) || !std::filesystem::is_directory(search_dir)) {
                return results;
            }

            std::string target_str(target);
            auto options = std::filesystem::directory_options::skip_permission_denied;
            std::vector<std::filesystem::path> all_files;

            for (const auto& entry : std::filesystem::recursive_directory_iterator(search_dir, options)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (filename.size() > 6 && filename.compare(filename.size() - 6, 6, ".smali") == 0) {
                        all_files.push_back(entry.path());
                    }
                }
            }

            std::mutex mtx;
            std::for_each(std::execution::par, all_files.begin(), all_files.end(), [&](const std::filesystem::path& file_path) {
                std::ifstream file(file_path);
                if (!file.is_open()) return;

                std::string line;
                size_t line_number = 1;
                std::string current_class = "";
                std::string current_method = "";
                std::vector<XrefResult> local_results;

                while (std::getline(file, line)) {
                    // Trim spaces da linha
                    size_t first = line.find_first_not_of(" \t\r\n");
                    size_t last = line.find_last_not_of(" \t\r\n");
                    if (first == std::string::npos) {
                        line_number++;
                        continue;
                    }
                    std::string trimmed_line = line.substr(first, last - first + 1);

                    // Rastreador de contexto estrutural (Agent-First)
                    if (trimmed_line.find(".class ") == 0) {
                        size_t class_start = trimmed_line.find_last_of(' ');
                        if (class_start != std::string::npos) {
                            current_class = trimmed_line.substr(class_start + 1);
                        }
                    } else if (trimmed_line.find(".method ") == 0) {
                        current_method = trimmed_line.substr(8);
                    } else if (trimmed_line == ".end method") {
                        current_method = "";
                    }
                    // Busca pelo Target (XREF)
                    else if (trimmed_line.find(target_str) != std::string::npos) {
                        local_results.push_back({
                            file_path,
                            current_class,
                            current_method,
                            line_number,
                            trimmed_line
                        });
                    }
                    line_number++;
                }

                if (!local_results.empty()) {
                    std::lock_guard<std::mutex> lock(mtx);
                    results.insert(results.end(), std::make_move_iterator(local_results.begin()), std::make_move_iterator(local_results.end()));
                }
            });
            return results;
        }
    };
}