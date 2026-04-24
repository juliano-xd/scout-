# 🗺️ Scout++ | Mapa Arquitetural e Funcionalidades

Este documento serve como referência técnica para a estrutura modular do Scout++. Cada motor (Engine) é projetado para ser independente, permitindo análise forense isolada ou orquestrada.

## 🚀 Motores de Análise (Engines)

| Funcionalidade | Descrição | Header (.hpp) | Implementação (.cpp) |
| :--- | :--- | :--- | :--- |
| **Content Search** | Busca textual/regex de alta performance. | include/engines/content_search_engine.hpp | src/engines/content_search_engine.cpp |
| **Class Search** | Localização de classes via Dalvik/String. | include/engines/class_search_engine.hpp | src/engines/class_search_engine.cpp |
| **XRef Engine** | Análise de referências cruzadas e grafos de chamada. | include/engines/xref_search_engine.hpp | src/engines/xref_search_engine.cpp |
| **Variable Tracker** | Taint Analysis (Rastreio de dados Source-to-Sink). | include/engines/variable_tracker_engine.hpp | src/engines/variable_tracker_engine.cpp |
| **CFG Engine** | Geração de grafos de controle de fluxo de métodos. | include/engines/cfg_engine.hpp | src/engines/cfg_engine.cpp |
| **UI Mapper** | Correlação de Resource IDs com bytecode. | include/engines/ui_mapper_engine.hpp | src/engines/ui_mapper_engine.cpp |
| **ResMap Engine** | Mapeamento de nomes de recursos (public.xml). | include/engines/resource_mapping_engine.hpp | src/engines/resource_mapping_engine.cpp |
| **Deobfuscation** | Detecção de Base64 e análise de entropia. | include/engines/deobf_engine.hpp | src/engines/deobf_engine.cpp |
| **Manifest Engine** | Auditoria de AndroidManifest.xml. | include/engines/manifest_engine.hpp | src/engines/manifest_engine.cpp |
| **Class Inspector** | Extração de DNA e metadados de classe. | include/engines/class_inspector_engine.hpp | src/engines/class_inspector_engine.cpp |
| **Dominator Anal.** | Cálculo de IPD para dissipação de fluxo. | include/engines/dominator_analyzer.hpp | src/engines/dominator_analyzer.cpp |

## 🏗️ Núcleo e Infraestrutura (Core)

| Componente | Função | Arquivo |
| :--- | :--- | :--- |
| **Main Loop** | Orquestração de CLI e Despacho de Motores. | src/main.cpp |
| **CLI Parser** | Sistema de flags e configuração (ScoutConfig). | include/cli/parser.hpp |
| **Scanner Core** | Motor de varredura e filtragem de diretórios. | include/core/scanner.hpp |
| **Contexto** | Estado global da análise e diretório raiz. | include/core/analysis_context.hpp |
| **S-Expr Formatter** | Protocolo de comunicação AI-Native. | src/formatters/sexpr_formatter.cpp |

## 🛠️ Bibliotecas Utilitárias (include/utils/)

*   **sexpr.hpp**: Lógica de construção de S-Expressions.
*   **mmap_file.hpp**: Performance extrema em leitura de arquivos gigantes.
*   **string_utils.hpp**: Helpers de texto, Base64 e entropia.
*   **filesystem.hpp**: Abstração de manipulação de arquivos do OS.

---
*Status: Scout++ Nível 16 - Estável e Documentado.*
