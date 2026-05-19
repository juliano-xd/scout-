(
  :|scout:rules|
  :version
  "1.0"
  :generated
  "2026-05-19T12:13:07Z"
  :source
  "RULES.md"
  :project
  (
    :project
    :name
    "Scout++"
    :tagline
    "Agent-First Forensic Framework"
    :language
    "C++26"
    :output-format
    "S-Expression"
    :purpose
    "Scout++ é uma framework forense Agent-First para análise estática de aplicativos Android (Dalvik/Smali)."
    :principles
    (
      (
        :principle
        :id
        1
        :text
        "S-Expression é o único formato de saída. Sem JSON, XML, YAML, protobuf ou qualquer intermediário."
        :type
        :SHOULD
      )
      (
        :principle
        :id
        2
        :text
        "Código deve ser autoexplicativo. Comentários são proibidos no código de produção — a estrutura, nomenclatura e organização substituem documentação inline."
        :type
        :MUST
      )
      (
        :principle
        :id
        3
        :text
        "Saída deve ser deterministicamente parseável. Dois agentes diferentes, sobre a mesma entrada, devem produzir árvores S-Expr idênticas."
        :type
        :SHOULD
      )
      (
        :principle
        :id
        4
        :text
        "Custo de tokens é prioridade. S-Expr é mais compacto que JSON (30-40% menos tokens para a mesma árvore), e o código deve refletir essa preocupação."
        :type
        :SHOULD
      )
    )
  )
  :foreword
  "# RULES.md — Constituição do Scout++ Este documento é a fonte única da verdade para qualquer agente de IA que interfira no código-fonte do Scout++. Sua leitura é obrigatória antes de qualquer alteração. Seu cumprimento é exigido durante toda modificação. Violações resultam em rejeição do commit e retrabalho."
  :sections
  (
    (
      :section
      :id
      "índice"
      :title
      "Índice"
      :content
      ((
        :ordered-list
        :items
        (
          "Natureza e Filosofia"
          "Arquitetura do Projeto"
          "Regras de Commit — A Lei"
          "Convenções de Código"
          "S-Expression — Especificação Formal"
          "TDD — Metodologia Obrigatória"
          "Fluxo de Trabalho do Agente"
          "Padrões de Projeto"
          "Segurança e Restrições"
          "Glossário"
        )
      ))
    )
    (
      :section
      :id
      "1"
      :title
      "1. Natureza e Filosofia"
      :subsections
      (
        (
          :subsection
          :id
          "1.1"
          :title
          "1.1 Propósito Fundamental"
          :body
          "Scout++ é uma framework forense Agent-First para análise estática de aplicativos Android (Dalvik/Smali).\n\nO programa atende dois públicos distintos e igualmente importantes:\n\nAmbos os casos de uso convergem na mesma necessidade: extrair significado semântico do bytecode Dalvik de forma precisa, performática e auditável."
          :content
          ((
            :unordered-list
            :items
            (
              "Analistas Forenses: investigação de malware, engenharia reversa de segurança, detecção de vazamento de dados, mapeamento de superfície de ataque, auditoria de conformidade."
              "Modders Avançados: modificação profunda de aplicativos mobile, compreensão de fluxos de dados, identificação de pontos de injeção, manipulação de lógica de negócio, bypass de restrições."
            )
          ))
        )
        (
          :subsection
          :id
          "1.2"
          :title
          "1.2 Filosofia \"Built by AIs, for AIs\""
          :body
          "O Scout++ é construído exclusivamente por agentes de IA e projetado exclusivamente para consumo por agentes de IA.\n\nConsequências diretas:"
          :content
          ((
            :ordered-list
            :items
            (
              "S-Expression é o único formato de saída. Sem JSON, XML, YAML, protobuf ou qualquer intermediário."
              "Código deve ser autoexplicativo. Comentários são proibidos no código de produção — a estrutura, nomenclatura e organização substituem documentação inline."
              "Saída deve ser deterministicamente parseável. Dois agentes diferentes, sobre a mesma entrada, devem produzir árvores S-Expr idênticas."
              "Custo de tokens é prioridade. S-Expr é mais compacto que JSON (30-40% menos tokens para a mesma árvore), e o código deve refletir essa preocupação."
            )
          ))
        )
        (
          :subsection
          :id
          "1.3"
          :title
          "1.3 Metodologia TDD — Intrínseca"
          :body
          "O projeto não existe sem testes. TDD não é uma sugestão, é a fundação."
          :content
          ((
            :unordered-list
            :items
            (
              "Toda funcionalidade nova deve ser precedida por testes que a validam (Red-Green-Refactor)."
              "Toda correção de bug deve ser acompanhada por um teste de regressão que falhava antes e passa depois."
              "Nenhum commit pode quebrar testes existentes."
              "Nenhum commit pode reduzir cobertura significativamente."
            )
          ))
        )
        (
          :subsection
          :id
          "1.4"
          :title
          "1.4 Comunicação Atemporal"
          :body
          "Commits funcionam como mensagens no tempo para outros agentes de IA que tocarão neste código no futuro.\n\nCada commit deve responder quatro perguntas:"
          :content
          ((
            :table
            :schema
            (
              "Pergunta"
              "Resposta"
            )
            :rows
            (
              (
                :row
                :Pergunta
                "O quê?"
                :Resposta
                "Quais arquivos, funções e estruturas foram modificados"
              )
              (
                :row
                :Pergunta
                "Por quê?"
                :Resposta
                "Qual bug, oportunidade ou requisito motivou a mudança"
              )
              (
                :row
                :Pergunta
                "Como?"
                :Resposta
                "Qual abordagem, algoritmo e trade-offs foram usados"
              )
              (
                :row
                :Pergunta
                "Quem?"
                :Resposta
                "Qual modelo de IA executou a modificação"
              )
            )
          ))
        )
      )
    )
    (
      :section
      :id
      "2"
      :title
      "2. Arquitetura do Projeto"
      :subsections
      (
        (
          :subsection
          :id
          "2.1"
          :title
          "2.1 Árvore de Diretórios"
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "/\n├── CMakeLists.txt          # Build system (C++26, FetchContent)\n├── Makefile                # Atalhos: make, make tests, make benchmark\n├── RULES.md                # ← Este documento\n├── CHANGELOG.html          # Relatório de modificações (monocromático)\n│\n├── include/                # Headers públicos — toda interface aqui\n│   ├── core/\n│   │   ├── analysis_context.hpp    # Contexto de análise (class loader)\n│   │   ├── i_search_engine.hpp     # Interface base de todos os motores\n│   │   └── search_config.hpp       # Configuração de busca unificada\n│   │\n│   ├── engines/\n│   │   ├── class_search/           # Busca de classes por nome\n│   │   ├── content_search/         # Busca textual e padrões\n│   │   ├── manifest/               # Parser de AndroidManifest.xml\n│   │   ├── class_inspector/        # \"DNA\" da classe\n│   │   ├── ui_mapper/              # Mapeamento de UI\n│   │   ├── deobf/                  # Detecção de ofuscação\n│   │   ├── smali_dump/             # Dump de AST Smali\n│   │   ├── cfg/                    # Grafo de Fluxo de Controle\n│   │   ├── xref_search/           # Referências cruzadas\n│   │   ├── variable_tracker/       # Taint Analysis (motor principal)\n│   │   └── register_engines.hpp    # Registry central\n│   │\n│   ├── utils/\n│   │   ├── sexpr.hpp              # AST nativa de S-Expression\n│   │   ├── string_utils.hpp       # Trim, split, escape\n│   │   └── filesystem.hpp         # Iteração em diretórios\n│   │\n│   ├── formatters/\n│   │   ├── i_formatter.hpp         # Interface de formatação\n│   │   ├── sexpr_formatter.hpp     # Formatador S-Expr\n│   │   └── formatter_registry.hpp  # Registry de formatadores\n│   │\n│   └── cli/\n│       └── parser.hpp              # Parser de argumentos CLI\n│\n├── src/                    # Implementação — headers mapeiam 1:1\n│   ├── core/\n│   ├── engines/\n│   │   ├── class_search/\n│   │   ├── content_search/\n│   │   ├── ...                    # Cada engine tem seu diretório\n│   │   └── register_engines.cpp\n│   ├── utils/\n│   └── formatters/\n│\n├── scripts/                # Ferramentas auxiliares (Python)\n│   └── taint_report.py     # Relatório HTML da taint analysis\n│\n├── tests/                  # Suítes de teste (Google Test)\n│\n└── build/                  # Artefatos (gitignored)"
          ))
        )
        (
          :subsection
          :id
          "2.2"
          :title
          "2.2 Motor de Taint Analysis — Arquitetura Interna"
          :body
          "O motor central (variable_tracker) segue esta arquitetura:"
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "┌──────────────────────────────────────────────────┐\n│                 VariableTrackerEngine             │\n│                                                   │\n│  ┌──────────────┐    ┌──────────────────────┐    │\n│  │   CacheKey    │    │    TrackingState      │    │\n│  │  method_sig   │    │  active_regs         │    │\n│  │  reg_mask     │    │  obj_taint_map       │    │\n│  │  control_hash │    │  control_taint_stack │    │\n│  └──────────────┘    │  static_fields_taint │    │\n│                       └──────────────────────┘    │\n│  ┌──────────────────────────────────────────────┐  │\n│  │            worklist (BFS/CFG)                │  │\n│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐   │  │\n│  │  │  block 0 │→│  block 1 │→│  block 2 │   │  │\n│  │  └──────────┘  └──────────┘  └──────────┘   │  │\n│  └──────────────────────────────────────────────┘  │\n│                                                   │\n│  string_pool_  ──  analysis_cache_  ──  stats_    │\n└──────────────────────────────────────────────────┘"
          ))
        )
        (
          :subsection
          :id
          "2.3"
          :title
          "2.3 Dependências Externas"
          :content
          ((
            :table
            :schema
            (
              "Dependência"
              "Uso"
              "Gerenciamento"
            )
            :rows
            (
              (
                :row
                :Dependência
                "Google Test"
                :Uso
                "Testes unitários"
                :Gerenciamento
                "FetchContent (CMake)"
              )
              (
                :row
                :Dependência
                "Google Benchmark"
                :Uso
                "Benchmarks"
                :Gerenciamento
                "FetchContent (CMake)"
              )
              (
                :row
                :Dependência
                "nlohmann/json"
                :Uso
                "PROIBIDO no pipeline de saída"
                :Gerenciamento
                "Removido — não reintroduzir"
              )
              (
                :row
                :Dependência
                "C++26 STL"
                :Uso
                "std::from_chars, std::ranges, std::views"
                :Gerenciamento
                "Compilador"
              )
            )
          ))
        )
      )
    )
    (
      :section
      :id
      "3"
      :title
      "3. Regras de Commit — A Lei"
      :subsections
      (
        (
          :subsection
          :id
          "3.1"
          :title
          "3.1 Estrutura Obrigatória"
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "<tipo>: <descrição concisa> (<código do ticket>)\n\n<corpo detalhado — mínimo 4 linhas, máximo 20 linhas>\n\nModelo-IA: <identificador> (v<versão>)"
          ))
        )
        (
          :subsection
          :id
          "3.2"
          :title
          "3.2 Tabela de Tipos"
          :content
          ((
            :table
            :schema
            (
              "Tipo"
              "Ticket"
              "Quando usar"
              "Exemplo"
            )
            :rows
            (
              (
                :row
                :Tipo
                "fix"
                :Ticket
                "C<N>"
                :|Quando usar|
                "Correção de bug que afeta corretude"
                :Exemplo
                "fix: preceded_ok ausente em devirtualize_call (C2)"
              )
              (
                :row
                :Tipo
                "perf"
                :Ticket
                "O<N>"
                :|Quando usar|
                "Otimização sem mudança de comportamento"
                :Exemplo
                "perf: pool_string com heterogeneous lookup (O1)"
              )
              (
                :row
                :Tipo
                "chore"
                :Ticket
                "L<N>"
                :|Quando usar|
                "Dead code, limpeza, sem mudança funcional"
                :Exemplo
                "chore: comentado alias_map (L2)"
              )
              (
                :row
                :Tipo
                "feat"
                :Ticket
                "F<N>"
                :|Quando usar|
                "Nova funcionalidade ou engine"
                :Exemplo
                "feat: script Python para relatorio HTML (H1)"
              )
              (
                :row
                :Tipo
                "docs"
                :Ticket
                "—"
                :|Quando usar|
                "Documentação"
                :Exemplo
                "docs: atualizado RULES.md"
              )
              (
                :row
                :Tipo
                "test"
                :Ticket
                "T<N>"
                :|Quando usar|
                "Adição ou correção de testes"
                :Exemplo
                "test: regressao para C2 devirtualize_call"
              )
              (
                :row
                :Tipo
                "refactor"
                :Ticket
                "R<N>"
                :|Quando usar|
                "Refatoração com mudança estrutural"
                :Exemplo
                "refactor: extraido merge_states (R1)"
              )
              (
                :row
                :Tipo
                "snapshot"
                :Ticket
                "—"
                :|Quando usar|
                "Estado intermediário para backup"
                :Exemplo
                "snapshot: antes das correcoes C1-C6"
              )
            )
          ))
        )
        (
          :subsection
          :id
          "3.3"
          :title
          "3.3 Códigos de Ticket"
          :body
          "Os números são sequenciais dentro de cada categoria. Nunca reutilizar um número de ticket."
          :content
          ((
            :unordered-list
            :items
            (
              "C1, C2, C3... — Correções (bugs na análise)"
              "O1, O2, O3... — Otimizações (performance)"
              "L1, L2, L3... — Limpeza (dead code)"
              "F1, F2, F3... — Funcionalidades (features novas)"
              "H1, H2, H3... — Ferramentas auxiliares (scripts, reports)"
              "T1, T2, T3... — Testes"
              "R1, R2, R3... — Refatorações"
            )
          ))
        )
        (
          :subsection
          :id
          "3.4"
          :title
          "3.4 Assinatura do Modelo"
          :body
          "A linha Modelo-IA é obrigatória e crítica para o projeto.\n\nFormato:\n\nExemplos:\n\nPor que isso é obrigatório?"
          :content
          (
            (
              :code-example
              :lang
              ""
              :code
              "Modelo-IA: <provedor>/<modelo> (v<data ou build>)"
            )
            (
              :code-example
              :lang
              ""
              :code
              "Modelo-IA: opencode/big-pickle (v20260518)\nModelo-IA: opencode/claude-sonnet-4 (v20260518)\nModelo-IA: opencode/gpt-4o (v20260518)\nModelo-IA: cursor/claude-sonnet-4-20250514 (v1)\nModelo-IA: github-copilot/claude-sonnet (v20260518)"
            )
            (
              :ordered-list
              :items
              (
                "Múltiplos modelos, múltiplas personalidades. Diferentes modelos têm diferentes pontos fortes, fraquezas e padrões de erro."
                "Rastreabilidade forense. Se um bug sistemático é identificado (ex: \"toda correção do modelo X esquece de atualizar o header\"), a assinatura permite identificar o padrão."
                "Comunicação atemporal. Um modelo que encontra código escrito por outro modelo pode adaptar sua abordagem sabendo qual \"mente\" produziu aquele código."
                "Crédito e responsabilidade. Cada IA assina seu trabalho."
              )
            )
          )
        )
        (
          :subsection
          :id
          "3.5"
          :title
          "3.5 Regras de Conteúdo"
          :content
          ((
            :table
            :schema
            (
              "Regra"
              "Descrição"
            )
            :rows
            (
              (
                :row
                :Regra
                "Atômico"
                :Descrição
                "Um commit = uma modificação lógica. Não acumular."
              )
              (
                :row
                :Regra
                "Idioma"
                :Descrição
                "Título: EN ou PT-BR. Corpo: PT-BR (público-alvo)."
              )
              (
                :row
                :Regra
                "What-Why-How"
                :Descrição
                "O que mudou, por que mudou, como foi implementado."
              )
              (
                :row
                :Regra
                "Impacto"
                :Descrição
                "O que muda em comportamento, performance ou segurança."
              )
              (
                :row
                :Regra
                "Testes"
                :Descrição
                "Mencionar quais testes foram criados/modificados/executados."
              )
              (
                :row
                :Regra
                "Regression"
                :Descrição
                "Confirmar que make tests passou."
              )
            )
          ))
        )
        (
          :subsection
          :id
          "3.6"
          :title
          "3.6 Exemplo de Commit Válido"
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "fix: adicionado preceded_ok + followed_ok no devirtualize_call (C2)\n\nA funcao devirtualize_call usava apenas header.find(method_sig) sem\nverificar se o nome do metodo era precedido por espaco/tab nem se era\nseguido por '(' (para nomes simples), causando falsos positivos quando\nmethod_sig era substring de outro nome de metodo no cabecalho.\n\nA correcao aplica o mesmo criterio estrito de track_recursive:\n- preceded_ok: espaco/tab antes do nome\n- followed_ok: '(' apos (nome simples) ou boundary (sig completa)\n- Suporte a assinaturas completas com parenteses (mesmo padrao C1)\n\nRegressao: class search + taint analysis validados em ~/wpp_decompiled.\n\nModelo-IA: opencode/big-pickle (v20260518)"
          ))
        )
        (
          :subsection
          :id
          "3.7"
          :title
          "3.7 Exemplo de Commit Inválido"
          :body
          "Problemas: sem descrição, sem ticket, sem assinatura, sem corpo, sem testes."
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "fix: varios bugs"
          ))
        )
        (
          :subsection
          :id
          "3.8"
          :title
          "3.8 Contagem de Commits"
          :body
          "Commits seguem numeração progressiva no changelog (CHANGELOG.html). A sequência atual (C1–C6, O1–O2, L1–L3, H1–H3) deve ser continuada."
        )
      )
    )
    (
      :section
      :id
      "4"
      :title
      "4. Convenções de Código"
      :subsections
      (
        (
          :subsection
          :id
          "4.1"
          :title
          "4.1 Padrão C++26"
          :body
          "O projeto alvo C++26 e faz uso ativo de:"
          :content
          ((
            :table
            :schema
            (
              "Recurso"
              "Onde usar"
              "Proibido"
            )
            :rows
            (
              (
                :row
                :Recurso
                "std::string_view"
                :|Onde usar|
                "Parâmetros de função, retornos"
                :Proibido
                "const std::string& como parâmetro de entrada"
              )
              (
                :row
                :Recurso
                "std::span"
                :|Onde usar|
                "Fatias de contêiner"
                :Proibido
                "Passagem de vector& para acesso só leitura"
              )
              (
                :row
                :Recurso
                "std::from_chars"
                :|Onde usar|
                "Parsing numérico"
                :Proibido
                "std::stoi, std::stod, std::atoi"
              )
              (
                :row
                :Recurso
                "std::ranges"
                :|Onde usar|
                "Algoritmos sobre coleções"
                :Proibido
                "Loops manuais quando ranges cobre o caso"
              )
              (
                :row
                :Recurso
                "std::views"
                :|Onde usar|
                "Transformações lazy"
                :Proibido
                "Alocações intermediárias"
              )
              (
                :row
                :Recurso
                "std::popcount, std::countr_zero"
                :|Onde usar|
                "Bitmasks (registers)"
                :Proibido
                "Loops manuais de bit"
              )
              (
                :row
                :Recurso
                "std::optional"
                :|Onde usar|
                "Valores opcionais"
                :Proibido
                "-1 ou nullptr como sentinela"
              )
              (
                :row
                :Recurso
                "noexcept"
                :|Onde usar|
                "Move operations, funções simples"
                :Proibido
                "Omitir noexcept onde aplicável"
              )
            )
          ))
        )
        (
          :subsection
          :id
          "4.2"
          :title
          "4.2 Naming Conventions"
          :content
          ((
            :table
            :schema
            (
              "Entidade"
              "Estilo"
              "Exemplo"
            )
            :rows
            (
              (
                :row
                :Entidade
                "Classes"
                :Estilo
                "PascalCase"
                :Exemplo
                "VariableTrackerEngine, CacheKey"
              )
              (
                :row
                :Entidade
                "Structs"
                :Estilo
                "PascalCase"
                :Exemplo
                "TrackingState, SearchResult"
              )
              (
                :row
                :Entidade
                "Funções"
                :Estilo
                "snake_case"
                :Exemplo
                "reg_to_bit, pool_string, merge_states"
              )
              (
                :row
                :Entidade
                "Métodos"
                :Estilo
                "snake_case"
                :Exemplo
                "track_recursive, process_method_internal"
              )
              (
                :row
                :Entidade
                "Variáveis"
                :Estilo
                "snake_case"
                :Exemplo
                "active_regs, control_taint_stack"
              )
              (
                :row
                :Entidade
                "Constantes"
                :Estilo
                "UPPER_SNAKE_CASE"
                :Exemplo
                "SmallSize, REG_NAMES"
              )
              (
                :row
                :Entidade
                "Enums"
                :Estilo
                "PascalCase"
                :Exemplo
                "EventAction"
              )
              (
                :row
                :Entidade
                "Enum values"
                :Estilo
                "UPPER_SNAKE_CASE"
                :Exemplo
                "CALL, LOAD, SINK_LEAK"
              )
              (
                :row
                :Entidade
                "Namespaces"
                :Estilo
                "snake_case"
                :Exemplo
                "engines, formatters, core"
              )
              (
                :row
                :Entidade
                "Membros privados"
                :Estilo
                "snake_case_ (sufixo _)"
                :Exemplo
                "string_pool_, analysis_cache_"
              )
              (
                :row
                :Entidade
                "Parâmetros"
                :Estilo
                "snake_case"
                :Exemplo
                "method_sig, virtual_method_sig"
              )
              (
                :row
                :Entidade
                "typedefs"
                :Estilo
                "PascalCase"
                :Exemplo
                "PointsToSet, VariableEvent"
              )
            )
          ))
        )
        (
          :subsection
          :id
          "4.3"
          :title
          "4.3 Arquivos"
          :body
          "Exemplo:"
          :content
          (
            (
              :unordered-list
              :items
              (
                "Headers: #pragma once. Um header por classe/struct principal."
                "Implementação: um .cpp por header, mesmo que pequeno."
                "Inclusões: ordenar: padrão → bibliotecas → projeto. Separar por linha em branco."
              )
            )
            (
              :code-example
              :lang
              "cpp"
              :code
              "#include <algorithm>\n#include <queue>\n#include <ranges>\n\n#include \"../../../include/core/analysis_context.hpp\"\n#include \"../../../include/utils/string_utils.hpp\""
            )
          )
        )
        (
          :subsection
          :id
          "4.4"
          :title
          "4.4 Proibição de Comentários"
          :body
          "Código de produção não deve conter comentários explicativos.\n\nPermitido:\n\nProibido:\n\nA estrutura do código, nomes de variáveis e organização devem tornar comentários desnecessários."
          :content
          (
            (
              :unordered-list
              :items
              (
                "// [BUG-N] — Marcador de bug rastreável"
                "// [PERF-N] — Marcador de otimização"
                "// [Ln] — Dead code comentado"
                "Comentários em arquivos de script (Python)"
              )
            )
            (
              :unordered-list
              :items
              (
                "// Explica o que a próxima linha faz"
                "// Este loop itera sobre..."
                "Docstrings Doxygen em métodos internos"
              )
            )
          )
        )
        (
          :subsection
          :id
          "4.5"
          :title
          "4.5 Tratamento de Erros"
          :content
          ((
            :unordered-list
            :items
            (
              "Preferir std::optional<T> a exceções para erros esperados."
              "Preferir std::expected<T, E> (C++23/26) para erros recuperáveis."
              "Exceções são aceitas apenas para erros fatais e inesperados."
              "from_chars com ec — não exceptions."
              "Retornar -1 ou {} apenas quando optional não é adequado (ex: desempenho crítico)."
            )
          ))
        )
        (
          :subsection
          :id
          "4.6"
          :title
          "4.6 Dead Code"
          :body
          "Código morto nunca é deletado. É comentado com / / e marcado com // [Ln]:\n\nIsso preserva o histórico intelectual enquanto remove o código do fluxo de compilação."
          :content
          ((
            :code-example
            :lang
            "cpp"
            :code
            "/* [L1] Dead code — CausalIntentSerializer nunca foi usado.\nclass CausalIntentSerializer {\n    ...\n};\n*/"
          ))
        )
        (
          :subsection
          :id
          "4.7"
          :title
          "4.7 Layout de Classe"
          :content
          ((
            :code-example
            :lang
            "cpp"
            :code
            "class NomeDaClasse : public InterfaceBase {\npublic:\n    // --- Tipos públicos ---\n    struct TrackingState { ... };\n\n    // --- Construtores ---\n    NomeDaClasse();\n\n    // --- Interface pública ---\n    std::vector<SearchResult> search(...) override;\n    std::string name() const override { return \"engine_name\"; }\n\n    // --- API pública adicional ---\n    static int funcao_auxiliar(...);\n\nprivate:\n    // --- Membros ---\n    std::unordered_set<std::string> string_pool_;\n\n    // --- Métodos privados ---\n    void metodo_interno(...);\n};"
          ))
        )
      )
    )
    (
      :section
      :id
      "5"
      :title
      "5. S-Expression — Especificação Formal"
      :subsections
      (
        (
          :subsection
          :id
          "5.1"
          :title
          "5.1 Gramática"
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "<sexpr>      ::= <atom> | <list>\n<atom>       ::= <string> | <integer> | <keyword> | <symbol> | <bool>\n<string>     ::= \\\" <char>* \\\"\n<integer>    ::= [0-9]+\n<keyword>    ::= : <symbol>\n<symbol>     ::= [a-zA-Z_-] [a-zA-Z0-9_-]*\n<bool>       ::= true | false\n<list>       ::= ( <sexpr>* )\n<kv-pair>    ::= <keyword> <sexpr>\n<form>       ::= ( <keyword> <kv-pair>* )"
          ))
        )
        (
          :subsection
          :id
          "5.2"
          :title
          "5.2 Regras de Serialização"
          :content
          ((
            :unordered-list
            :items
            (
              "Strings: \\\"conteudo\\\". Caracteres especiais escapados: \\n, \\t, \\\\, \\\"."
              "Keywords: prefixo :, sem aspas: :total, :version, :results."
              "Integers: decimais sem zeros à esquerda: 42, não 042."
              "Lists: (item1 item2 item3)."
              "Nested: (:result :file \\\"Main.smali\\\" :line 42)."
              "Pretty-print: indentação opcional para consumo humano; --machine-sexpr desativa."
            )
          ))
        )
        (
          :subsection
          :id
          "5.3"
          :title
          "5.3 Saída Padrão do Scout"
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "(:scout:<tipo>\n  :timestamp \"2026-05-18T10:00:00Z\"\n  :version \"<versão>\"\n  :total <N>\n  :results (<resultados>...)\n  :engine \"<nome_do_motor>\")"
          ))
        )
        (
          :subsection
          :id
          "5.4"
          :title
          "5.4 Formatos Específicos"
          :body
          "Search Result:\n\nTaint Analysis Report:\n\nCFG:"
          :content
          (
            (
              :code-example
              :lang
              ""
              :code
              "(:result :file \"path/Classe.smali\" :line 42 :content \"linha\" :context \"(:sexpr-contexto...)\")"
            )
            (
              :code-example
              :lang
              ""
              :code
              "(:aero-taint-report\n  :start \"Lcom/example;->method()V\"\n  :flow ((:ev :m \"...\" :l 5 :r \"p0\" :a \"CALL\" :t \"...\" :e \"IMPLICIT\") ...))"
            )
            (
              :code-example
              :lang
              ""
              :code
              "(:cfg-report\n  :method \"Lcom/example;->method()V\"\n  :blocks ((:block :id 0 :successors (1 2) :code \"...\" :ipd -1) ...))"
            )
          )
        )
        (
          :subsection
          :id
          "5.5"
          :title
          "5.5 Proibições"
          :content
          ((
            :unordered-list
            :items
            (
              "Sem JSON em qualquer ponto do pipeline de saída."
              "Sem conversão JSON → S-Expr em qualquer camada."
              "Sem bibliotecas externas de S-Expr. Usar sexpr.hpp nativa."
            )
          ))
        )
      )
    )
    (
      :section
      :id
      "6"
      :title
      "6. TDD — Metodologia Obrigatória"
      :subsections
      (
        (
          :subsection
          :id
          "6.1"
          :title
          "6.1 Ciclo Red-Green-Refactor"
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "┌──────────┐     ┌──────────┐     ┌──────────┐\n│   RED    │────→│  GREEN   │────→│ REFACTOR │\n│ Escrever │     │ Passar   │     │ Melhorar │\n│ teste    │     │ teste    │     │ código   │\n│ que falha│     │          │     │          │\n└──────────┘     └──────────┘     └──────────┘"
          ))
        )
        (
          :subsection
          :id
          "6.2"
          :title
          "6.2 Obrigações"
          :content
          ((
            :table
            :schema
            (
              "Obrigação"
              "Descrição"
            )
            :rows
            (
              (
                :row
                :Obrigação
                "Teste antes do código"
                :Descrição
                "Funcionalidade nova começa com teste."
              )
              (
                :row
                :Obrigação
                "Teste de regressão"
                :Descrição
                "Bug fix começa com teste que reproduz o bug."
              )
              (
                :row
                :Obrigação
                "100% dos testes passando"
                :Descrição
                "Antes de qualquer commit."
              )
              (
                :row
                :Obrigação
                "Sem #if 0 para desabilitar testes"
                :Descrição
                "Testes desabilitados são deletados ou corrigidos."
              )
              (
                :row
                :Obrigação
                "Cobertura mínima"
                :Descrição
                "Funções novas devem ter ao menos 1 teste positivo e 1 negativo."
              )
            )
          ))
        )
        (
          :subsection
          :id
          "6.3"
          :title
          "6.3 Execução"
          :content
          ((
            :code-example
            :lang
            "bash"
            :code
            "# Todos os testes\nmake tests\n\n# Teste específico\n./build/test_smali_dump --gtest_filter=SmaliDumpEngine.*\n./build/test_xref_search --gtest_filter=*Massive*\n\n# Verificar sem recompilar\n./build/test_all"
          ))
        )
        (
          :subsection
          :id
          "6.4"
          :title
          "6.4 Estrutura de Testes"
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "tests/\n├── test_core/               # Testes do core framework\n├── test_engines/            # Testes por engine\n│   ├── test_class_search.cpp\n│   ├── test_content_search.cpp\n│   ├── test_cfg.cpp\n│   ├── test_xref_search_engine.cpp\n│   └── ...\n├── test_utils/              # Testes de utilitários\n├── massive/                 # Testes de carga (muitos arquivos)\n└── audit/                   # Testes de auditoria forense"
          ))
        )
      )
    )
    (
      :section
      :id
      "7"
      :title
      "7. Fluxo de Trabalho do Agente"
      :subsections
      (
        (
          :subsection
          :id
          "7.1"
          :title
          "7.1 Ciclo de Modificação"
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "1. LER RULES.md\n       │\n2. ANALISAR ──→ código existente\n       │           testes existentes\n       │           propósito do componente\n       │\n3. PLANEJAR ──→ o que mudar\n       │           impacto da mudança\n       │           testes necessários\n       │\n4. MODIFICAR ──→ implementar mudança\n       │\n5. COMPILAR ──→ cmake --build build -j$(nproc)\n       │           SEM WARNINGS. SEM ERROS.\n       │\n6. TESTAR ──→ make tests (ou testes específicos)\n       │           TODOS VERDES.\n       │\n7. VALIDAR ──→ smoke test no APK real\n       │           ./build/scout -p ~/wpp_decompiled --search \"Lcom/whatsapp/Main;\"\n       │           ./build/scout -p ~/wpp_decompiled --track-var \"...:p0\" --track-depth 2\n       │\n8. COMMIT ──→ seguir formato obrigatório\n       │           assinar com modelo\n       │\n9. CHANGELOG ──→ regenerar ou atualizar CHANGELOG.html\n       │\n10. FINALIZAR ──→ confirmar que tudo está em ordem"
          ))
        )
        (
          :subsection
          :id
          "7.2"
          :title
          "7.2 Prioridades na Tomada de Decisão"
          :content
          ((
            :table
            :schema
            (
              "Prioridade"
              "Critério"
              "Exemplos"
            )
            :rows
            (
              (
                :row
                :Prioridade
                "P0"
                :Critério
                "Corretude da análise"
                :Exemplos
                "Dados errados são piores que dados lentos"
              )
              (
                :row
                :Prioridade
                "P1"
                :Critério
                "Estabilidade"
                :Exemplos
                "Memory leaks, crashes, UB"
              )
              (
                :row
                :Prioridade
                "P2"
                :Critério
                "Performance"
                :Exemplos
                "Análise deve completar em tempo viável"
              )
              (
                :row
                :Prioridade
                "P3"
                :Critério
                "Legibilidade"
                :Exemplos
                "Código deve ser compreensível por outros agentes"
              )
              (
                :row
                :Prioridade
                "P4"
                :Critério
                "Estilo"
                :Exemplos
                "Convenções, formatação"
              )
            )
          ))
        )
        (
          :subsection
          :id
          "7.3"
          :title
          "7.3 Validação no APK Real"
          :body
          "O APK de teste é o WhatsApp decompilado em ~/wpp_decompiled/ (~77k arquivos smali).\n\nValidação mínima:"
          :content
          ((
            :code-example
            :lang
            "bash"
            :code
            "# 1. Class search (deve encontrar Main.smali)\n./build/scout -p ~/wpp_decompiled --search \"Lcom/whatsapp/Main;\" | grep -q \"Main.smali\"\n\n# 2. Taint analysis (deve detectar sink)\ntimeout 15 ./build/scout -p ~/wpp_decompiled \\\n  --track-var \"Lcom/whatsapp/registration/app/phonenumberentry/RegisterPhone;->onSuccess()V:p0\" \\\n  --track-depth 2 | grep -q \"SINK_LEAK\""
          ))
        )
      )
    )
    (
      :section
      :id
      "8"
      :title
      "8. Padrões de Projeto"
      :subsections
      (
        (
          :subsection
          :id
          "8.1"
          :title
          "8.1 Registry Pattern (Engines e Formatters)"
          :body
          "Engines e formatters são registrados via static initialization:\n\nNão chamar funções create_* manualmente. O registro é automático."
          :content
          ((
            :code-example
            :lang
            "cpp"
            :code
            "// register_engines.hpp\ntemplate <typename T>\nstruct EngineRegistrar {\n    EngineRegistrar(const std::string& name) {\n        EngineRegistry::instance().register_engine(name, []() {\n            return std::make_unique<T>();\n        });\n    }\n};"
          ))
        )
        (
          :subsection
          :id
          "8.2"
          :title
          "8.2 Engine Pattern"
          :body
          "Toda engine segue:"
          :content
          ((
            :code-example
            :lang
            "cpp"
            :code
            "class MinhaEngine : public ISearchEngine {\npublic:\n    // Metadados\n    std::string name() const override;\n    std::string description() const override;\n\n    // Execução\n    std::vector<SearchResult> search(\n        core::AnalysisContext& ctx,\n        const SearchConfig& config\n    ) override;\n\n    // Suporte\n    bool supports_config(const SearchConfig& config) const override;\n\n    // Estatísticas\n    EngineStats get_stats() const override;\n};"
          ))
        )
        (
          :subsection
          :id
          "8.3"
          :title
          "8.3 String Pool Pattern"
          :body
          "Para evitar alocações repetidas, strings são \"internadas\" em um pool:"
          :content
          ((
            :code-example
            :lang
            "cpp"
            :code
            "std::set<std::string, std::less<>> string_pool_;  // O1: heterogeneous lookup\n\nstd::string_view pool_string(std::string_view s) {\n    auto it = string_pool_.find(s);\n    if (it != string_pool_.end()) return *it;\n    return *string_pool_.insert(std::string(s)).first;\n}"
          ))
        )
        (
          :subsection
          :id
          "8.4"
          :title
          "8.4 Cache Pattern (Taint Analysis)"
          :body
          "Resultados de análise são cacheados para evitar retrabalho:"
          :content
          ((
            :code-example
            :lang
            "cpp"
            :code
            "struct CacheKey {\n    std::string_view method_sig;\n    uint64_t reg_mask;\n    uint64_t control_hash;  // C5: adicionado para contextos de controle\n};\n\nstd::unordered_map<CacheKey, std::pair<std::vector<VariableEvent>, MethodSummary>, CacheKeyHash> analysis_cache_;"
          ))
        )
        (
          :subsection
          :id
          "8.5"
          :title
          "8.5 PointsToSet — Small Object Optimization"
          :body
          "Usa união com small buffer (2 elementos) para evitar heap allocation na maioria dos casos:"
          :content
          ((
            :code-example
            :lang
            "cpp"
            :code
            "union {\n    LocusID small[SmallSize];  // 2 elementos = sem heap\n    std::vector<LocusID>* large;  // >2 elementos = heap\n} storage;"
          ))
        )
        (
          :subsection
          :id
          "8.6"
          :title
          "8.6 Worklist Algorithm (Taint Analysis)"
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "1. Construir CFG do método\n2. Estado de entrada do bloco 0 = estado inicial\n3. Para cada bloco na worklist:\n   a. Executar função de transferência (process_method_internal)\n   b. Propagar saída para sucessores (merge_states)\n   c. Se estado do sucessor mudou, adicionar à worklist\n4. Repetir até convergência (worklist vazia)"
          ))
        )
        (
          :subsection
          :id
          "8.7"
          :title
          "8.7 Análise Interprocedural"
          :content
          ((
            :code-example
            :lang
            ""
            :code
            "track_recursive:\n  1. Verificar cache (CacheKey)\n  2. Verificar ciclo (in_progress_methods_)\n  3. Analisar método atual (worklist)\n  4. Para cada CALL:\n     a. Resolver target (devirtualize_call)\n     b. Chamar track_recursive recursivamente\n     c. Propagar retorno\n  5. Armazenar no cache\n  6. Retornar MethodSummary"
          ))
        )
      )
    )
    (
      :section
      :id
      "9"
      :title
      "9. Segurança e Restrições"
      :subsections
      (
        (
          :subsection
          :id
          "9.1"
          :title
          "9.1 Proibições Absolutas"
          :content
          ((
            :table
            :schema
            (
              "Item"
              "Motivo"
            )
            :rows
            (
              (
                :row
                :Item
                "Push sem autorização"
                :Motivo
                "O usuário decide quando publicar"
              )
              (
                :row
                :Item
                "git push --force"
                :Motivo
                "Destrutivo, perde histórico"
              )
              (
                :row
                :Item
                "git reset --hard"
                :Motivo
                "Destrutivo, perde trabalho"
              )
              (
                :row
                :Item
                "Modificar build/"
                :Motivo
                "Artefatos de compilação, gitignored"
              )
              (
                :row
                :Item
                "Commit de secrets"
                :Motivo
                ".env, chaves, senhas — nunca"
              )
              (
                :row
                :Item
                "Comitar sem testes"
                :Motivo
                "Viola TDD"
              )
              (
                :row
                :Item
                "Reintroduzir JSON no output"
                :Motivo
                "Filosofia Agent-First"
              )
              (
                :row
                :Item
                "Deletar código sem comentar"
                :Motivo
                "Dead code deve ser preservado"
              )
            )
          ))
        )
        (
          :subsection
          :id
          "9.2"
          :title
          "9.2 Thread Safety"
          :body
          "Motores podem ser chamados concorrentemente."
          :content
          ((
            :unordered-list
            :items
            (
              "Estado global deve ser const ou protegido por mutex."
              "string_pool_ é local por instância do engine."
              "analysis_cache_ é local por instância do engine."
              "Evitar singletons mutáveis."
            )
          ))
        )
        (
          :subsection
          :id
          "9.3"
          :title
          "9.3 Memory Safety"
          :content
          ((
            :unordered-list
            :items
            (
              "std::string_view só é retornado se o storage subjacente sobreviver."
              "Pool de strings (string_pool_) garante que views inseridas permaneçam válidas durante a chamada search()."
              "Limpar o pool entre chamadas (C6): seguro pois cada search() é independente."
              "PointsToSet com union requer cuidado com membro ativo."
            )
          ))
        )
        (
          :subsection
          :id
          "9.4"
          :title
          "9.4 Undefined Behavior — Checklist"
          :body
          "Antes de commitar, verificar:"
          :content
          ((
            :checklist
            :items
            (
              (
                :item
                :id
                1
                :text
                "Nenhum acesso a string_view após o pool ser limpo"
                :done
                false
              )
              (
                :item
                :id
                2
                :text
                "Nenhuma leitura de membro inativo de union"
                :done
                false
              )
              (
                :item
                :id
                3
                :text
                "Nenhum std::stoi sem try/catch (ou melhor, use from_chars)"
                :done
                false
              )
              (
                :item
                :id
                4
                :text
                "Nenhuma comparação de string_view com string que aloca"
                :done
                false
              )
              (
                :item
                :id
                5
                :text
                "Nenhum iterador inválido após inserção em unordered_map"
                :done
                false
              )
              (
                :item
                :id
                6
                :text
                "Nenhuma referência pendente para contêiner realocado"
                :done
                false
              )
            )
          ))
        )
      )
    )
    (
      :section
      :id
      "10"
      :title
      "10. Glossário"
      :content
      ((
        :table
        :schema
        (
          "Termo"
          "Definição"
        )
        :rows
        (
          (
            :row
            :Termo
            "Agent-First"
            :Definição
            "Projetado para consumo primário por agentes de IA, não humanos"
          )
          (
            :row
            :Termo
            "CFG"
            :Definição
            "Control Flow Graph — grafo de blocos básicos de um método"
          )
          (
            :row
            :Termo
            "CHA"
            :Definição
            "Class Hierarchy Analysis — resolução de chamadas virtuais"
          )
          (
            :row
            :Termo
            "Dalvik"
            :Definição
            "Formato de bytecode Android (.smali)"
          )
          (
            :row
            :Termo
            "FCFG"
            :Definição
            "FlowCFG — CFG com informação de fluxo de dados excepcional"
          )
          (
            :row
            :Termo
            "IPD"
            :Definição
            "Immediate Post-Dominator — nó que domina imediatamente outro no CFG"
          )
          (
            :row
            :Termo
            "LocusID"
            :Definição
            "Identificador compacto (32 bits) de um sítio de alocação"
          )
          (
            :row
            :Termo
            "PEI"
            :Definição
            "Potentially Excepting Instruction — instrução que pode lançar exceção"
          )
          (
            :row
            :Termo
            "S-Expr"
            :Definição
            "S-Expression — formato de representação de árvore de dados"
          )
          (
            :row
            :Termo
            "Sink"
            :Definição
            "Ponto de vazamento de dados (ex: SharedPreferences.edit())"
          )
          (
            :row
            :Termo
            "Smali"
            :Definição
            "Representação textual do bytecode Dalvik"
          )
          (
            :row
            :Termo
            "Source"
            :Definição
            "Origem de dado sensível"
          )
          (
            :row
            :Termo
            "Taint"
            :Definição
            "Dado marcado como sensível ou contaminado"
          )
          (
            :row
            :Termo
            "TDD"
            :Definição
            "Test-Driven Development — desenvolvimento orientado a testes"
          )
          (
            :row
            :Termo
            "Worklist"
            :Definição
            "Conjunto de blocos do CFG pendentes de processamento"
          )
          (
            :row
            :Termo
            "XREF"
            :Definição
            "Cross-reference — referência cruzada entre métodos/classes"
          )
        )
      ))
    )
    (
      :section
      :id
      "apêndice-a-—-checklist-do-agente"
      :title
      "Apêndice A — Checklist do Agente"
      :body
      "Antes de finalizar qualquer modificação, o agente deve confirmar:"
      :content
      (
        (
          :code-example
          :lang
          ""
          :code
          "[ ] 1. Leu RULES.md completo\n[ ] 2. Entendeu o código existente (não modificou às cegas)\n[ ] 3. Rodou compilação: cmake --build build -j$(nproc) — sem erros, sem warnings\n[ ] 4. Rodou testes: make tests — todos verdes\n[ ] 5. Rodou smoke test no APK real — class search + taint analysis\n[ ] 6. Commit segue formato obrigatório com assinatura do modelo\n[ ] 7. CHANGELOG.html foi atualizado (se aplicável)\n[ ] 8. Nenhuma das proibições da Seção 9 foi violada\n[ ] 9. Nenhum comentário foi adicionado ao código de produção\n[ ] 10. O código segue as convenções de estilo da Seção 4"
        )
        (
          :quote
          :text
          "\"Built by AIs, for AIs. Commits as atemporal messages. Output in S-Expr only.\""
        )
      )
    )
  )
)
