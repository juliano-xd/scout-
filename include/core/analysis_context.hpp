#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <filesystem>
#include <memory>
#include <mutex>
#include <algorithm>
#include <fstream>
#include "utils/mmap_file.hpp"
#include "utils/string_utils.hpp"

namespace core {

    /**
     * @brief Contexto central de análise que gerencia recursos compartilhados entre motores.
     * Implementa o padrão Registry e Resource Cache para garantir Zero-Redundancy em I/O.
     */
    class AnalysisContext {
    public:
        AnalysisContext(const std::filesystem::path& root_dir) : root_dir_(root_dir) {
            initialize_smali_index();
        }

        // Proibir cópia para garantir integridade dos recursos mmap'd
        AnalysisContext(const AnalysisContext&) = delete;
        AnalysisContext& operator=(const AnalysisContext&) = delete;

        /**
         * @brief Obtém o conteúdo de uma classe Smali via Memory Map.
         * Se o arquivo já estiver mapeado, retorna o view existente (Zero-Copy).
         */
        std::string_view get_class_content(std::string_view class_name) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto path = resolve_class_path(class_name);
            if (path.empty()) return "";

            auto it = mmap_cache_.find(path.string());
            if (it != mmap_cache_.end()) {
                return it->second->view();
            }

            auto mmap = std::make_unique<utils::MappedFile>(path);
            if (!mmap->is_open()) return "";

            std::string_view view = mmap->view();
            mmap_cache_[path.string()] = std::move(mmap);
            return view;
        }

        const std::filesystem::path& root_dir() const { return root_dir_; }

    private:
        std::filesystem::path root_dir_;
        std::unordered_map<std::string, std::filesystem::path> class_index_;
        std::unordered_map<std::string, std::unique_ptr<utils::MappedFile>> mmap_cache_;
        std::mutex mutex_;

        void initialize_smali_index() {
            if (!std::filesystem::exists(root_dir_) || !std::filesystem::is_directory(root_dir_)) return;

            for (const auto& entry : std::filesystem::recursive_directory_iterator(root_dir_)) {
                if (entry.is_regular_file() && entry.path().extension() == ".smali") {
                    // Abrir arquivo para ler o cabeçalho .class
                    std::ifstream file(entry.path());
                    std::string line;
                    while (std::getline(file, line)) {
                        line = utils::trim(line);
                        if (line.starts_with(".class ")) {
                            size_t last_space = line.find_last_of(' ');
                            if (last_space != std::string::npos) {
                                std::string dalvik_name = std::string(line.substr(last_space + 1));
                                // Limpar possíveis comentários ou espaços
                                if (!dalvik_name.empty()) {
                                    class_index_[dalvik_name] = entry.path();
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }

        std::filesystem::path resolve_class_path(std::string_view class_name) {
            std::string name(class_name);
            auto it = class_index_.find(name);
            if (it != class_index_.end()) return it->second;
            return "";
        }
    };

} // namespace core
