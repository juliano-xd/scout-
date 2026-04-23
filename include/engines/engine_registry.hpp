#pragma once

#include "i_search_engine.hpp"
#include <memory>
#include <unordered_map>
#include <functional>
#include <string>

namespace engines {

    /**
     * @brief Registry central para criação e gerenciamento de motores de busca.
     * Implementa o padrão Factory + Registry para permitir extensão dinâmica.
     */
    class EngineRegistry {
    public:
        using CreatorFunc = std::function<std::unique_ptr<ISearchEngine>()>;

        /**
         * @brief Obtém a instância singleton do registry.
         */
        static EngineRegistry& instance() {
            static EngineRegistry registry;
            return registry;
        }

        /**
         * @brief Registra um novo tipo de motor no sistema.
         * @param engine_name Nome único do motor (ex: "class", "content")
         * @param creator Função que cria uma instância do motor
         */
        void register_engine(const std::string& engine_name, CreatorFunc creator) {
            if (engine_name.empty()) {
                throw std::invalid_argument("Engine name cannot be empty");
            }
            std::lock_guard<std::mutex> lock(mutex_);
            if (creators_.find(engine_name) != creators_.end()) {
                throw std::runtime_error("Engine '" + engine_name + "' already registered");
            }
            creators_[engine_name] = std::move(creator);
        }

        /**
         * @brief Cria uma instância de um motor pelo nome.
         * @param engine_name Nome do motor registrado
         * @return Unique pointer para o motor criado, ou nullptr se não encontrado
         */
        std::unique_ptr<ISearchEngine> create(const std::string& engine_name) const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = creators_.find(engine_name);
            if (it == creators_.end()) {
                return nullptr;
            }
            return it->second();
        }

        /**
         * @brief Lista todos os nomes de motores registrados.
         */
        std::vector<std::string> list_engines() const {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<std::string> names;
            names.reserve(creators_.size());
            for (const auto& [name, _] : creators_) {
                names.push_back(name);
            }
            return names;
        }

        /**
         * @brief Verifica se um motor está registrado.
         */
        bool has_engine(const std::string& engine_name) const {
            std::lock_guard<std::mutex> lock(mutex_);
            return creators_.find(engine_name) != creators_.end();
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::string, CreatorFunc> creators_;

        EngineRegistry() = default;
        ~EngineRegistry() = default;
        EngineRegistry(const EngineRegistry&) = delete;
        EngineRegistry& operator=(const EngineRegistry&) = delete;
    };

    /**
     * @brief Helper para registro automático de motores.
     * Usa static initialization para registrar motores automaticamente.
     * 
     * Uso: static EngineRegistrar<ClassSearchEngine> registrar("class");
     */
    template<typename EngineType>
    class EngineRegistrar {
    public:
        explicit EngineRegistrar(const std::string& name) {
            EngineRegistry::instance().register_engine(name, []() {
                return std::make_unique<EngineType>();
            });
        }
    };

} // namespace engines