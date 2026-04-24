#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <regex>
#include <execution>
#include <mutex>

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
     * @brief Verifica se um caminho deve ser incluído na busca com base nas configurações de diretório.
     */
    inline bool is_path_filtered(const std::filesystem::path& p, const std::vector<std::string>& include_dirs, const std::vector<std::string>& exclude_dirs) {
        if (include_dirs.empty() && exclude_dirs.empty()) return true;

        std::string path_str = p.generic_string();
        
        // Se houver inclusões, o caminho deve conter pelo menos uma delas
        if (!include_dirs.empty()) {
            bool found = false;
            for (const auto& inc : include_dirs) {
                if (path_str.find(inc) != std::string::npos) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }

        // Se houver exclusões, o caminho não deve conter nenhuma delas
        for (const auto& exc : exclude_dirs) {
            if (path_str.find(exc) != std::string::npos) {
                return false;
            }
        }

        return true;
    }

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

        // Optimized Fast path: Check common Smali directory structures directly
        static const std::string_view common_dirs[] = {"smali", "smali_classes2", "smali_classes3", "smali_classes4", "smali_classes5"};
        for (auto d : common_dirs) {
            std::filesystem::path p = search_dir / d / info.package_path / info.file_name;
            if (std::filesystem::exists(p)) return p;
        }

        // Generic recursive scan for non-standard structures
        for (const auto& entry : std::filesystem::recursive_directory_iterator(search_dir, options)) {
            if (entry.is_regular_file() && entry.path().filename() == info.file_name) {
                // Verify package if not empty
                if (info.package_path.empty()) return entry.path();
                std::string path_str = entry.path().generic_string();
                if (path_str.find(info.package_path) != std::string::npos) return entry.path();
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Busca todos os arquivos .smali que contenham o texto especificado no nome da classe.
     */
    inline std::vector<std::filesystem::path> find_classes_containing(
        const std::filesystem::path& search_dir, 
        std::string_view query,
        const std::vector<std::string>& include_dirs = {},
        const std::vector<std::string>& exclude_dirs = {}
    ) {
        std::vector<std::filesystem::path> results;
        if (!std::filesystem::exists(search_dir)) return results;

        auto options = std::filesystem::directory_options::skip_permission_denied;
        std::mutex mtx;

        std::vector<std::filesystem::path> files_to_check;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(search_dir, options)) {
            if (entry.is_regular_file() && entry.path().extension() == ".smali") {
                if (is_path_filtered(entry.path(), include_dirs, exclude_dirs)) {
                    files_to_check.push_back(entry.path());
                }
            }
        }

        std::for_each(std::execution::par, files_to_check.begin(), files_to_check.end(), [&](const std::filesystem::path& p) {
            std::string path_str = p.generic_string();
            if (path_str.find(query) != std::string::npos) {
                std::lock_guard<std::mutex> lock(mtx);
                results.push_back(p);
            }
        });

        return results;
    }

    /**
     * @brief Varre todos os arquivos .smali recursivamente e aplica uma função em paralelo.
     */
    template<typename Func>
    inline void scan_files(
        const std::filesystem::path& search_dir, 
        Func callback,
        const std::vector<std::string>& include_dirs = {},
        const std::vector<std::string>& exclude_dirs = {}
    ) {
        if (!std::filesystem::exists(search_dir)) return;
        auto options = std::filesystem::directory_options::skip_permission_denied;
        
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(search_dir, options)) {
            if (entry.is_regular_file() && entry.path().extension() == ".smali") {
                if (is_path_filtered(entry.path(), include_dirs, exclude_dirs)) {
                    files.push_back(entry.path());
                }
            }
        }

        std::for_each(std::execution::par, files.begin(), files.end(), [&](const std::filesystem::path& p) {
            callback(p);
        });
    }

}
