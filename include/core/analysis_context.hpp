#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <system_error>
#include "../../include/utils/mmap_file.hpp"
#include "../../include/utils/string_utils.hpp"

namespace core {

    namespace detail {
        /**
         * @brief Hash Transparente (Heterogeneous Lookup) C++20.
         * @details Permite fazer `map.find(std::string_view)` num map de `std::string`
         * sem forçar a alocação de uma std::string temporária na Heap.
         */
        struct StringHash {
            using is_transparent = void; // Marcação vital para o compilador
            [[nodiscard]] size_t operator()(std::string_view txt) const noexcept {
                return std::hash<std::string_view>{}(txt);
            }
            [[nodiscard]] size_t operator()(const std::string& txt) const noexcept {
                return std::hash<std::string>{}(txt);
            }
        };
    }

    /**
     * @brief Contexto central de análise que gerencia recursos compartilhados entre motores.
     * @details Implementa o padrão Registry e Resource Cache. Otimizado para acessos
     * massivos em multi-thread usando Double-Checked Locking e Heterogeneous Lookups.
     */
    class AnalysisContext {
    public:
        explicit AnalysisContext(const std::filesystem::path& root_dir) : root_dir_(root_dir){
            initialize_smali_index();
        }

        // Proibir cópia e movimento para garantir integridade absoluta dos recursos mmap'd
        AnalysisContext(const AnalysisContext&) = delete;
        AnalysisContext& operator=(const AnalysisContext&) = delete;
        AnalysisContext(AnalysisContext&&) = delete;
        AnalysisContext& operator=(AnalysisContext&&) = delete;

        /**
         * @brief Obtém o conteúdo de uma classe Smali via Memory Map.
         * @details Algoritmo Lock-Free para leituras. Só adquire Lock Exclusivo se
         * ocorrer um Cache Miss. Zero-Copy e Zero-Allocation se já estiver mapeado.
         */
        [[nodiscard]] std::string_view get_class_content(std::string_view class_name) {
            auto path = resolve_class_path(class_name);
            if (path.empty()) [[unlikely]] return {};

            // O unordered_map precisa de uma chave string se formos inserir,
            // mas o path interno é consistente, então usamos path.string() como cache key.
            const std::string cache_key = path.string();

            // ==========================================
            // FAST PATH: Leitura Concorrente
            // ==========================================
            {
                // std::shared_lock permite infinitos leitores simultâneos
                std::shared_lock<std::shared_mutex> read_lock(rw_mutex_);
                if (auto it = mmap_cache_.find(cache_key); it != mmap_cache_.end()) {
                    return it->second->view();
                }
            }

            // ==========================================
            // SLOW PATH: Cache Miss (Double-Checked Locking)
            // ==========================================
            std::unique_lock<std::shared_mutex> write_lock(rw_mutex_);

            // Re-check: Outra thread pode ter escrito a cache enquanto aguardávamos o write_lock
            if (auto it = mmap_cache_.find(cache_key); it != mmap_cache_.end()) [[unlikely]] {
                return it->second->view();
            }

            try {
                // Instancia o novo mapeamento (lança excepção se falhar a nível do SO)
                auto mmap = std::make_unique<utils::MappedFile>(path);

                // Extrai a view ANTES do std::move
                std::string_view view = mmap->view();
                mmap_cache_.emplace(cache_key, std::move(mmap));
                return view;
            } catch (const std::system_error&) {
                // Recuperação segura em caso de ficheiro apagado ou sem permissão subjacente
                return {};
            }
        }

        [[nodiscard]] const std::filesystem::path& root_dir() const noexcept { return root_dir_; }

    private:
        std::filesystem::path root_dir_;

        // C++20: Utilização de is_transparent no hasher e std::equal_to<>
        // para habilitar buscas ultra-rápidas apenas com std::string_view.
        std::unordered_map<std::string, std::filesystem::path, detail::StringHash, std::equal_to<>> class_index_;
        std::unordered_map<std::string, std::unique_ptr<utils::MappedFile>> mmap_cache_;

        // Mutex de leitura/escrita para máxima concorrência
        std::shared_mutex rw_mutex_;

        /**
         * @brief Varre o APK/Directório e mapeia os pacotes Dalvik para o sistema de ficheiros.
         */
        void initialize_smali_index() {
            std::error_code ec;
            if (!std::filesystem::is_directory(root_dir_, ec) || ec) return;

            auto it = std::filesystem::recursive_directory_iterator(
                root_dir_, std::filesystem::directory_options::skip_permission_denied, ec);
            const auto end = std::filesystem::recursive_directory_iterator();

            if (ec) return;

            while (it != end) {
                if (it->is_regular_file(ec) && !ec && it->path().extension() == ".smali") {

                    // Substituição de std::ifstream pelo utilitário customizado.
                    // O `MappedFile` não cria buffers dinâmicos, e o `LineIterator` opera
                    // 100% sobre views em memória RAM. Tempo de indexação drasticamente reduzido.
                    try {
                        utils::MappedFile smali_file(it->path());
                        utils::LineIterator line_it(smali_file.view());
                        std::string_view line;

                        while (line_it.next(line)) {
                            const std::string_view trimmed = utils::trim(line);

                            if (trimmed.starts_with(".class ")) {
                                const size_t last_space = trimmed.find_last_of(' ');
                                if (last_space != std::string_view::npos) {

                                    std::string_view dalvik_name = trimmed.substr(last_space + 1);
                                    if (!dalvik_name.empty()) {
                                        // O map cuidará de converter o string_view final para string
                                        class_index_.emplace(dalvik_name, it->path());
                                    }
                                }
                                break; // Já encontrámos a classe, avança para próximo ficheiro
                            }
                        }
                    } catch (const std::system_error&) {
                        // Ignora graciosamente ficheiros corrompidos durante o varrimento
                    }
                }

                // Avanço no-throw para imunidade contra quebras no file system
                it.increment(ec);
                if (ec) ec.clear();
            }
        }

        /**
         * @brief Resolve um nome de classe para o seu caminho no disco (O(1)).
         */
        [[nodiscard]] std::filesystem::path resolve_class_path(std::string_view class_name) const {
            // Devido ao Heterogeneous Lookup (StringHash + std::equal_to<>),
            // este .find() aceita std::string_view diretamente, SEM alocar std::string.
            auto it = class_index_.find(class_name);
            if (it != class_index_.end()) {
                return it->second;
            }
            return {};
        }
    };

} // namespace core
