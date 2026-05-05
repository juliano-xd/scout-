#pragma once

#include <filesystem>
#include <string_view>
#include <span>          // C++20/26: O padrão para blocos de memória contíguos
#include <system_error>
#include <utility>       // Para std::exchange
#include <cstddef>       // Para std::byte

// Headers POSIX
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @file mapped_file.hpp
 * @brief Wrapper RAII para mapeamento de ficheiros em memória via POSIX mmap.
 * @details Optimização de footprint (16 bytes). Lança std::system_error em caso de falha,
 * garantindo que o objecto nunca existe num estado inválido ("zombie state").
 */
namespace utils {

    class MappedFile {
    public:
        /**
         * @brief Constrói e mapeia o ficheiro em memória.
         * @param path Caminho para o ficheiro.
         * @throws std::system_error contendo o código `errno` se a operação no SO falhar.
         */
        explicit MappedFile(const std::filesystem::path& path) {
            // O_CLOEXEC: Previne que o FD seja herdado por processos filhos após um fork/exec
            int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
            if (fd == -1) [[unlikely]] {
                throw std::system_error(errno, std::generic_category(), "Failed to open file: " + path.string());
            }

            struct stat st;
            if (fstat(fd, &st) == -1) [[unlikely]] {
                close(fd);
                throw std::system_error(errno, std::generic_category(), "Failed to stat file: " + path.string());
            }

            size_ = static_cast<size_t>(st.st_size);

            if (size_ == 0) {
                // Ficheiro vazio é legítimo, mas não fazemos mmap.
                close(fd);
                return;
            }

            // PROT_READ (apenas leitura) com MAP_PRIVATE (copy-on-write, sem afectar o ficheiro real)
            data_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, 0);

            // OPTIMIZAÇÃO CRÍTICA:
            // A chamada `mmap` mantém a sua própria referência ao inode.
            // Podemos e DEVEMOS fechar o FD imediatamente para evitar o esgotamento de FDs no SO.
            close(fd);

            if (data_ == MAP_FAILED) [[unlikely]] {
                data_ = nullptr;
                size_ = 0;
                throw std::system_error(errno, std::generic_category(), "Failed to mmap file: " + path.string());
            }
        }

        /**
         * @brief Destrutor RAII para libertar a memória mapeada.
         */
        ~MappedFile() {
            if (data_) {
                munmap(data_, size_);
            }
        }

        // Bloqueia a cópia do objecto (não faz sentido duplicar handles de recursos SO desta forma)
        MappedFile(const MappedFile&) = delete;
        MappedFile& operator=(const MappedFile&) = delete;

        /**
         * @brief Move Constructor (Semântica de Movimento C++26).
         * @details Utiliza `std::exchange` para anular o objecto de origem numa única operação.
         */
        MappedFile(MappedFile&& other) noexcept
            : data_(std::exchange(other.data_, nullptr)),
              size_(std::exchange(other.size_, 0)) {}

        /**
         * @brief Move Assignment.
         */
        MappedFile& operator=(MappedFile&& other) noexcept {
            if (this != &other) [[likely]] {
                if (data_) {
                    munmap(data_, size_);
                }
                data_ = std::exchange(other.data_, nullptr);
                size_ = std::exchange(other.size_, 0);
            }
            return *this;
        }

        // ==========================================
        // Accessors (Garantem imutabilidade e zero-cost)
        // ==========================================

        [[nodiscard]] bool is_empty() const noexcept { return size_ == 0; }
        [[nodiscard]] size_t size() const noexcept { return size_; }

        /**
         * @brief Retorna uma view de texto (C++17 string_view).
         * @details Útil se o ficheiro mapeado for um ficheiro de texto plano (ex: JSON, XML, S-Expr).
         */
        [[nodiscard]] std::string_view view() const noexcept {
            if (!data_) return {};
            return {static_cast<const char*>(data_), size_};
        }

        /**
         * @brief Retorna uma view de bytes (C++20 std::span).
         * @details É a abordagem state-of-the-art para processamento de ficheiros binários/raw.
         */
        [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
            if (!data_) return {};
            return {static_cast<const std::byte*>(data_), size_};
        }

    private:
        // Tamanho da classe foi reduzido, o int fd_ já não é necessário.
        void* data_ = nullptr;
        size_t size_ = 0;
    };

} // namespace utils
