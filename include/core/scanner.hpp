#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <regex>

namespace core {

    struct ClassInfo {
        std::string package_path;
        std::string class_name;
        std::string file_name;
        
        static ClassInfo parse(std::string_view dalvik_name) {
            std::string name(dalvik_name);
            // Remove 'L' prefix and ';' suffix se presentes
            if (name.size() >= 2 && name.front() == 'L' && name.back() == ';') {
                name = name.substr(1, name.size() - 2);
            }
            // Substitui '.' por '/' para suportar tanto nomeação java quanto dalvik
            std::replace(name.begin(), name.end(), '.', '/');
            
            ClassInfo info;
            auto slash_pos = name.find_last_of('/');
            if (slash_pos != std::string::npos) {
                info.package_path = name.substr(0, slash_pos);
                info.class_name = name.substr(slash_pos + 1);
            } else {
                info.package_path = "";
                info.class_name = name;
            }
            info.file_name = info.class_name + ".smali";
            return info;
        }
    };

    /**
     * @brief Busca o arquivo .smali correspondente a uma classe Dalvik (Lcom/ex/A;) ou dot-notation (com.ex.A)
     * @param search_dir Diretório base onde a busca inicia
     * @param dalvik_name Nome da classe a ser buscada
     * @return Caminho completo para o arquivo .smali, ou nullopt se não for encontrado
     */
    inline std::optional<std::filesystem::path> find_class_file(const std::filesystem::path& search_dir, std::string_view dalvik_name) {
        if (!std::filesystem::exists(search_dir) || !std::filesystem::is_directory(search_dir)) {
            return std::nullopt;
        }

        auto info = ClassInfo::parse(dalvik_name);
        auto options = std::filesystem::directory_options::skip_permission_denied;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(search_dir, options)) {
            if (entry.is_regular_file() && entry.path().filename() == info.file_name) {
                // Se a classe tem pacote, garante que o caminho da pasta bate com o pacote
                if (!info.package_path.empty()) {
                    std::string expected_suffix = info.package_path + "/" + info.file_name;
                    std::string path_str = entry.path().generic_string(); // Usa '/' independente do OS
                    
                    if (path_str.size() >= expected_suffix.size() && 
                        path_str.compare(path_str.size() - expected_suffix.size(), expected_suffix.size(), expected_suffix) == 0) {
                        return entry.path();
                    }
                } else {
                    // Sem pacote, encontrou a classe
                    return entry.path();
                }
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Busca todos os arquivos .smali que contenham o texto especificado no nome da classe.
     * @param search_dir Diretório base onde a busca inicia
     * @param query Texto a ser buscado no nome da classe (case sensitive)
     * @return Vetor de caminhos para os arquivos encontrados
     */
    inline std::vector<std::filesystem::path> find_classes_containing(const std::filesystem::path& search_dir, std::string_view query) {
        std::vector<std::filesystem::path> results;
        if (!std::filesystem::exists(search_dir) || !std::filesystem::is_directory(search_dir)) {
            return results;
        }

        std::string query_str(query);
        auto options = std::filesystem::directory_options::skip_permission_denied;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(search_dir, options)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.size() > 6 && filename.compare(filename.size() - 6, 6, ".smali") == 0) {
                    std::string class_name = filename.substr(0, filename.size() - 6);
                    if (class_name.find(query_str) != std::string::npos) {
                        results.push_back(entry.path());
                    }
                }
            }
        }
        return results;
    }

    struct MatchResult {
        std::filesystem::path file_path;
        size_t line_number;
        std::string line_content;
    };

    /**
     * @brief Busca conteúdo interno (string literal ou regex) nos arquivos .smali.
     * @param search_dir Diretório base onde a busca inicia
     * @param query Texto ou regex a ser buscado
     * @param type Tipo de busca ("string", "regex", etc.)
     * @return Vetor de ocorrências contendo arquivo, linha e texto correspondente.
     */
    inline std::vector<MatchResult> search_content(const std::filesystem::path& search_dir, std::string_view query, std::string_view type) {
        std::vector<MatchResult> results;
        if (!std::filesystem::exists(search_dir) || !std::filesystem::is_directory(search_dir)) {
            return results;
        }

        std::string query_str(query);
        std::regex pattern;
        bool use_regex = false;

        if (type == "regex") {
            try {
                pattern = std::regex(query_str);
                use_regex = true;
            } catch (...) {
                return results;
            }
        }

        auto options = std::filesystem::directory_options::skip_permission_denied;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(search_dir, options)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.size() > 6 && filename.compare(filename.size() - 6, 6, ".smali") == 0) {
                    std::ifstream file(entry.path());
                    if (!file.is_open()) continue;

                    std::string line;
                    size_t line_number = 1;
                    while (std::getline(file, line)) {
                        bool matched = false;
                        if (use_regex) {
                            if (std::regex_search(line, pattern)) {
                                matched = true;
                            }
                        } else if (type == "string") {
                            size_t const_pos = line.find("const-string");
                            if (const_pos != std::string::npos) {
                                size_t start_quote = line.find('"', const_pos);
                                size_t end_quote = line.rfind('"');
                                if (start_quote != std::string::npos && end_quote != std::string::npos && end_quote > start_quote) {
                                    std::string_view content(line.data() + start_quote + 1, end_quote - start_quote - 1);
                                    if (content.find(query_str) != std::string_view::npos) {
                                        matched = true;
                                    }
                                }
                            }
                        } else {
                            if (line.find(query_str) != std::string::npos) {
                                matched = true;
                            }
                        }

                        if (matched) {
                            size_t first = line.find_first_not_of(" \t\r\n");
                            size_t last = line.find_last_not_of(" \t\r\n");
                            std::string trimmed_line = (first != std::string::npos && last != std::string::npos) ? line.substr(first, last - first + 1) : line;
                            
                            results.push_back({entry.path(), line_number, trimmed_line});
                        }
                        line_number++;
                    }
                }
            }
        }
        return results;
    }
}
