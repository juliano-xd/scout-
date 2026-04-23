#pragma once

#include "i_formatter.hpp"
#include <memory>
#include <unordered_map>
#include <functional>
#include <string>
#include <mutex>

namespace formatters {

    /**
     * @brief Registry central para criação e gerenciamento de formatadores de saída.
     * Implementa o padrão Factory + Registry para permitir extensão dinâmica.
     */
    class FormatterRegistry {
    public:
        using CreatorFunc = std::function<std::unique_ptr<IFormatter>()>;

        /**
         * @brief Obtém a instância singleton do registry.
         */
        static FormatterRegistry& instance() {
            static FormatterRegistry registry;
            return registry;
        }

        /**
         * @brief Registra um novo tipo de formatador no sistema.
         * @param formatter_name Nome único do formatador (ex: "sexpr", "json", "text")
         * @param creator Função que cria uma instância do formatador
         */
        void register_formatter(const std::string& formatter_name, CreatorFunc creator) {
            if (formatter_name.empty()) {
                throw std::invalid_argument("Formatter name cannot be empty");
            }
            std::lock_guard<std::mutex> lock(mutex_);
            if (creators_.find(formatter_name) != creators_.end()) {
                throw std::runtime_error("Formatter '" + formatter_name + "' already registered");
            }
            creators_[formatter_name] = std::move(creator);
        }

        /**
         * @brief Cria uma instância de um formatador pelo nome.
         * @param formatter_name Nome do formatador registrado
         * @return Unique pointer para o formatador criado, ou nullptr se não encontrado
         */
        std::unique_ptr<IFormatter> create(const std::string& formatter_name) const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = creators_.find(formatter_name);
            if (it == creators_.end()) {
                return nullptr;
            }
            return it->second();
        }

        /**
         * @brief Lista todos os nomes de formatadores registrados.
         */
        std::vector<std::string> list_formatters() const {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<std::string> names;
            names.reserve(creators_.size());
            for (const auto& [name, _] : creators_) {
                names.push_back(name);
            }
            return names;
        }

        /**
         * @brief Verifica se um formatador está registrado.
         */
        bool has_formatter(const std::string& formatter_name) const {
            std::lock_guard<std::mutex> lock(mutex_);
            return creators_.find(formatter_name) != creators_.end();
        }

        /**
         * @brief Obtém o formatador padrão (primeiro registrado ou "sexpr").
         */
        std::unique_ptr<IFormatter> create_default() const {
            std::lock_guard<std::mutex> lock(mutex_);
            if (creators_.empty()) {
                return nullptr;
            }
            // Preferir "sexpr" como padrão se disponível
            auto it = creators_.find("sexpr");
            if (it != creators_.end()) {
                return it->second();
            }
            // Caso contrário, usar o primeiro da lista
            return creators_.begin()->second();
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::string, CreatorFunc> creators_;

        FormatterRegistry() = default;
        ~FormatterRegistry() = default;
        FormatterRegistry(const FormatterRegistry&) = delete;
        FormatterRegistry& operator=(const FormatterRegistry&) = delete;
    };

    /**
     * @brief Helper para registro automático de formatadores.
     * Usa static initialization para registrar formatadores automaticamente.
     * 
     * Uso: static FormatterRegistrar<SExprFormatter> registrar("sexpr");
     */
    template<typename FormatterType>
    class FormatterRegistrar {
    public:
        explicit FormatterRegistrar(const std::string& name) {
            FormatterRegistry::instance().register_formatter(name, []() {
                return std::make_unique<FormatterType>();
            });
        }
    };

} // namespace formatters