# RULES.md — Constituição do Scout++

> Este documento é a **fonte única da verdade** para qualquer agente de IA que interfira no código-fonte do Scout++.  
> Sua leitura é **obrigatória antes de qualquer alteração**. Seu cumprimento é **exigido durante toda modificação**.  
> Violações resultam em **rejeição do commit** e **retrabalho**.

---

## Índice

1. [Natureza e Filosofia](#1-natureza-e-filosofia)
2. [Arquitetura do Projeto](#2-arquitetura-do-projeto)
3. [Regras de Commit — A Lei](#3-regras-de-commit--a-lei)
4. [Convenções de Código](#4-convenções-de-código)
5. [S-Expression — Especificação Formal](#5-sexpression--especificação-formal)
6. [TDD — Metodologia Obrigatória](#6-tdd--metodologia-obrigatória)
7. [Fluxo de Trabalho do Agente](#7-fluxo-de-trabalho-do-agente)
8. [Padrões de Projeto](#8-padrões-de-projeto)
9. [Segurança e Restrições](#9-segurança-e-restrições)
10. [Glossário](#10-glossário)

---

## 1. Natureza e Filosofia

### 1.1 Propósito Fundamental

Scout++ é uma **framework forense Agent-First** para análise estática de aplicativos Android (Dalvik/Smali).

O programa atende dois públicos distintos e igualmente importantes:

- **Analistas Forenses**: investigação de malware, engenharia reversa de segurança, detecção de vazamento de dados, mapeamento de superfície de ataque, auditoria de conformidade.
- **Modders Avançados**: modificação profunda de aplicativos mobile, compreensão de fluxos de dados, identificação de pontos de injeção, manipulação de lógica de negócio, bypass de restrições.

Ambos os casos de uso convergem na mesma necessidade: **extrair significado semântico do bytecode Dalvik de forma precisa, performática e auditável**.

### 1.2 Filosofia "Built by AIs, for AIs"

O Scout++ é construído **exclusivamente** por agentes de IA e projetado **exclusivamente** para consumo por agentes de IA.

Consequências diretas:

1. **S-Expression é o único formato de saída**. Sem JSON, XML, YAML, protobuf ou qualquer intermediário.
2. **Código deve ser autoexplicativo**. Comentários são proibidos no código de produção — a estrutura, nomenclatura e organização substituem documentação inline.
3. **Saída deve ser deterministicamente parseável**. Dois agentes diferentes, sobre a mesma entrada, devem produzir árvores S-Expr idênticas.
4. **Custo de tokens é prioridade**. S-Expr é mais compacto que JSON (30-40% menos tokens para a mesma árvore), e o código deve refletir essa preocupação.

### 1.3 Metodologia TDD — Intrínseca

O projeto **não existe sem testes**. TDD não é uma sugestão, é a fundação.

- **Toda** funcionalidade nova deve ser precedida por testes que a validam (Red-Green-Refactor).
- **Toda** correção de bug deve ser acompanhada por um teste de regressão que falhava antes e passa depois.
- **Nenhum commit** pode quebrar testes existentes.
- **Nenhum commit** pode reduzir cobertura significativamente.

### 1.4 Comunicação Atemporal

Commits funcionam como **mensagens no tempo** para outros agentes de IA que tocarão neste código no futuro.

Cada commit deve responder **quatro perguntas**:

| Pergunta | Resposta |
|----------|----------|
| **O quê?** | Quais arquivos, funções e estruturas foram modificados |
| **Por quê?** | Qual bug, oportunidade ou requisito motivou a mudança |
| **Como?** | Qual abordagem, algoritmo e trade-offs foram usados |
| **Quem?** | Qual modelo de IA executou a modificação |

---

## 2. Arquitetura do Projeto

### 2.1 Árvore de Diretórios

```
/
├── CMakeLists.txt          # Build system (C++26, FetchContent)
├── Makefile                # Atalhos: make, make tests, make benchmark
├── RULES.md                # ← Este documento
├── CHANGELOG.html          # Relatório de modificações (monocromático)
│
├── include/                # Headers públicos — toda interface aqui
│   ├── core/
│   │   ├── analysis_context.hpp    # Contexto de análise (class loader)
│   │   ├── i_search_engine.hpp     # Interface base de todos os motores
│   │   └── search_config.hpp       # Configuração de busca unificada
│   │
│   ├── engines/
│   │   ├── class_search/           # Busca de classes por nome
│   │   ├── content_search/         # Busca textual e padrões
│   │   ├── manifest/               # Parser de AndroidManifest.xml
│   │   ├── class_inspector/        # "DNA" da classe
│   │   ├── ui_mapper/              # Mapeamento de UI
│   │   ├── deobf/                  # Detecção de ofuscação
│   │   ├── smali_dump/             # Dump de AST Smali
│   │   ├── cfg/                    # Grafo de Fluxo de Controle
│   │   ├── xref_search/           # Referências cruzadas
│   │   ├── variable_tracker/       # Taint Analysis (motor principal)
│   │   └── register_engines.hpp    # Registry central
│   │
│   ├── utils/
│   │   ├── sexpr.hpp              # AST nativa de S-Expression
│   │   ├── string_utils.hpp       # Trim, split, escape
│   │   └── filesystem.hpp         # Iteração em diretórios
│   │
│   ├── formatters/
│   │   ├── i_formatter.hpp         # Interface de formatação
│   │   ├── sexpr_formatter.hpp     # Formatador S-Expr
│   │   └── formatter_registry.hpp  # Registry de formatadores
│   │
│   └── cli/
│       └── parser.hpp              # Parser de argumentos CLI
│
├── src/                    # Implementação — headers mapeiam 1:1
│   ├── core/
│   ├── engines/
│   │   ├── class_search/
│   │   ├── content_search/
│   │   ├── ...                    # Cada engine tem seu diretório
│   │   └── register_engines.cpp
│   ├── utils/
│   └── formatters/
│
├── scripts/                # Ferramentas auxiliares (Python)
│   └── taint_report.py     # Relatório HTML da taint analysis
│
├── tests/                  # Suítes de teste (Google Test)
│
└── build/                  # Artefatos (gitignored)
```

### 2.2 Motor de Taint Analysis — Arquitetura Interna

O motor central (`variable_tracker`) segue esta arquitetura:

```
┌──────────────────────────────────────────────────┐
│                 VariableTrackerEngine             │
│                                                   │
│  ┌──────────────┐    ┌──────────────────────┐    │
│  │   CacheKey    │    │    TrackingState      │    │
│  │  method_sig   │    │  active_regs         │    │
│  │  reg_mask     │    │  obj_taint_map       │    │
│  │  control_hash │    │  control_taint_stack │    │
│  └──────────────┘    │  static_fields_taint │    │
│                       └──────────────────────┘    │
│  ┌──────────────────────────────────────────────┐  │
│  │            worklist (BFS/CFG)                │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐   │  │
│  │  │  block 0 │→│  block 1 │→│  block 2 │   │  │
│  │  └──────────┘  └──────────┘  └──────────┘   │  │
│  └──────────────────────────────────────────────┘  │
│                                                   │
│  string_pool_  ──  analysis_cache_  ──  stats_    │
└──────────────────────────────────────────────────┘
```

### 2.3 Dependências Externas

| Dependência | Uso | Gerenciamento |
|-------------|-----|---------------|
| Google Test | Testes unitários | FetchContent (CMake) |
| Google Benchmark | Benchmarks | FetchContent (CMake) |
| nlohmann/json | **PROIBIDO no pipeline de saída** | Removido — não reintroduzir |
| C++26 STL | `std::from_chars`, `std::ranges`, `std::views` | Compilador |

---

## 3. Regras de Commit — A Lei

### 3.1 Estrutura Obrigatória

```
<tipo>: <descrição concisa> (<código do ticket>)

<corpo detalhado — mínimo 4 linhas, máximo 20 linhas>

Modelo-IA: <identificador> (v<versão>)
```

### 3.2 Tabela de Tipos

| Tipo | Ticket | Quando usar | Exemplo |
|------|--------|-------------|---------|
| `fix` | `C<N>` | Correção de bug que afeta corretude | `fix: preceded_ok ausente em devirtualize_call (C2)` |
| `perf` | `O<N>` | Otimização sem mudança de comportamento | `perf: pool_string com heterogeneous lookup (O1)` |
| `chore` | `L<N>` | Dead code, limpeza, sem mudança funcional | `chore: comentado alias_map (L2)` |
| `feat` | `F<N>` | Nova funcionalidade ou engine | `feat: script Python para relatorio HTML (H1)` |
| `docs` | — | Documentação | `docs: atualizado RULES.md` |
| `test` | `T<N>` | Adição ou correção de testes | `test: regressao para C2 devirtualize_call` |
| `refactor` | `R<N>` | Refatoração com mudança estrutural | `refactor: extraido merge_states (R1)` |
| `snapshot` | — | Estado intermediário para backup | `snapshot: antes das correcoes C1-C6` |

### 3.3 Códigos de Ticket

- **C1, C2, C3...** — Correções (bugs na análise)
- **O1, O2, O3...** — Otimizações (performance)
- **L1, L2, L3...** — Limpeza (dead code)
- **F1, F2, F3...** — Funcionalidades (features novas)
- **H1, H2, H3...** — Ferramentas auxiliares (scripts, reports)
- **T1, T2, T3...** — Testes
- **R1, R2, R3...** — Refatorações

Os números são sequenciais dentro de cada categoria. **Nunca reutilizar** um número de ticket.

### 3.4 Assinatura do Modelo

A linha `Modelo-IA` é **obrigatória** e **crítica** para o projeto.

Formato:
```
Modelo-IA: <provedor>/<modelo> (v<data ou build>)
```

Exemplos:

```
Modelo-IA: opencode/big-pickle (v20260518)
Modelo-IA: opencode/claude-sonnet-4 (v20260518)
Modelo-IA: opencode/gpt-4o (v20260518)
Modelo-IA: cursor/claude-sonnet-4-20250514 (v1)
Modelo-IA: github-copilot/claude-sonnet (v20260518)
```

**Por que isso é obrigatório?**

1. **Múltiplos modelos**, múltiplas personalidades. Diferentes modelos têm diferentes pontos fortes, fraquezas e padrões de erro.
2. **Rastreabilidade forense**. Se um bug sistemático é identificado (ex: "toda correção do modelo X esquece de atualizar o header"), a assinatura permite identificar o padrão.
3. **Comunicação atemporal**. Um modelo que encontra código escrito por outro modelo pode adaptar sua abordagem sabendo qual "mente" produziu aquele código.
4. **Crédito e responsabilidade**. Cada IA assina seu trabalho.

### 3.5 Regras de Conteúdo

| Regra | Descrição |
|-------|-----------|
| **Atômico** | Um commit = uma modificação lógica. Não acumular. |
| **Idioma** | Título: EN ou PT-BR. Corpo: PT-BR (público-alvo). |
| **What-Why-How** | O que mudou, por que mudou, como foi implementado. |
| **Impacto** | O que muda em comportamento, performance ou segurança. |
| **Testes** | Mencionar quais testes foram criados/modificados/executados. |
| **Regression** | Confirmar que `make tests` passou. |

### 3.6 Exemplo de Commit Válido

```
fix: adicionado preceded_ok + followed_ok no devirtualize_call (C2)

A funcao devirtualize_call usava apenas header.find(method_sig) sem
verificar se o nome do metodo era precedido por espaco/tab nem se era
seguido por '(' (para nomes simples), causando falsos positivos quando
method_sig era substring de outro nome de metodo no cabecalho.

A correcao aplica o mesmo criterio estrito de track_recursive:
- preceded_ok: espaco/tab antes do nome
- followed_ok: '(' apos (nome simples) ou boundary (sig completa)
- Suporte a assinaturas completas com parenteses (mesmo padrao C1)

Regressao: class search + taint analysis validados em ~/wpp_decompiled.

Modelo-IA: opencode/big-pickle (v20260518)
```

### 3.7 Exemplo de Commit Inválido

```
fix: varios bugs
```
**Problemas**: sem descrição, sem ticket, sem assinatura, sem corpo, sem testes.

### 3.8 Contagem de Commits

Commits seguem **numeração progressiva** no changelog (`CHANGELOG.html`).
A sequência atual (`C1`–`C6`, `O1`–`O2`, `L1`–`L3`, `H1`–`H3`) deve ser continuada.

---

## 4. Convenções de Código

### 4.1 Padrão C++26

O projeto alvo **C++26** e faz uso ativo de:

| Recurso | Onde usar | Proibido |
|---------|-----------|----------|
| `std::string_view` | Parâmetros de função, retornos | `const std::string&` como parâmetro de entrada |
| `std::span` | Fatias de contêiner | Passagem de `vector&` para acesso só leitura |
| `std::from_chars` | Parsing numérico | `std::stoi`, `std::stod`, `std::atoi` |
| `std::ranges` | Algoritmos sobre coleções | Loops manuais quando `ranges` cobre o caso |
| `std::views` | Transformações lazy | Alocações intermediárias |
| `std::popcount`, `std::countr_zero` | Bitmasks (registers) | Loops manuais de bit |
| `std::optional` | Valores opcionais | `-1` ou `nullptr` como sentinela |
| `noexcept` | Move operations, funções simples | Omitir `noexcept` onde aplicável |

### 4.2 Naming Conventions

| Entidade | Estilo | Exemplo |
|----------|--------|---------|
| Classes | `PascalCase` | `VariableTrackerEngine`, `CacheKey` |
| Structs | `PascalCase` | `TrackingState`, `SearchResult` |
| Funções | `snake_case` | `reg_to_bit`, `pool_string`, `merge_states` |
| Métodos | `snake_case` | `track_recursive`, `process_method_internal` |
| Variáveis | `snake_case` | `active_regs`, `control_taint_stack` |
| Constantes | `UPPER_SNAKE_CASE` | `SmallSize`, `REG_NAMES` |
| Enums | `PascalCase` | `EventAction` |
| Enum values | `UPPER_SNAKE_CASE` | `CALL`, `LOAD`, `SINK_LEAK` |
| Namespaces | `snake_case` | `engines`, `formatters`, `core` |
| Membros privados | `snake_case_` (sufixo `_`) | `string_pool_`, `analysis_cache_` |
| Parâmetros | `snake_case` | `method_sig`, `virtual_method_sig` |
| typedefs | `PascalCase` | `PointsToSet`, `VariableEvent` |

### 4.3 Arquivos

- **Headers**: `#pragma once`. Um header por classe/struct principal.
- **Implementação**: um `.cpp` por header, mesmo que pequeno.
- **Inclusões**: ordenar: padrão → bibliotecas → projeto. Separar por linha em branco.

Exemplo:
```cpp
#include <algorithm>
#include <queue>
#include <ranges>

#include "../../../include/core/analysis_context.hpp"
#include "../../../include/utils/string_utils.hpp"
```

### 4.4 Proibição de Comentários

Código de produção **não deve conter comentários explicativos**.

**Permitido**:
- `// [BUG-N]` — Marcador de bug rastreável
- `// [PERF-N]` — Marcador de otimização
- `// [Ln]` — Dead code comentado
- Comentários em arquivos de script (Python)

**Proibido**:
- `// Explica o que a próxima linha faz`
- `// Este loop itera sobre...`
- Docstrings Doxygen em métodos internos

A estrutura do código, nomes de variáveis e organização devem tornar comentários desnecessários.

### 4.5 Tratamento de Erros

- Preferir `std::optional<T>` a exceções para erros esperados.
- Preferir `std::expected<T, E>` (C++23/26) para erros recuperáveis.
- Exceções são aceitas apenas para erros **fatais** e **inesperados**.
- `from_chars` com `ec` — não exceptions.
- Retornar `-1` ou `{}` apenas quando `optional` não é adequado (ex: desempenho crítico).

### 4.6 Dead Code

Código morto **nunca é deletado**. É **comentado com `/* */`** e marcado com `// [Ln]`:

```cpp
/* [L1] Dead code — CausalIntentSerializer nunca foi usado.
class CausalIntentSerializer {
    ...
};
*/
```

Isso preserva o histórico intelectual enquanto remove o código do fluxo de compilação.

### 4.7 Layout de Classe

```cpp
class NomeDaClasse : public InterfaceBase {
public:
    // --- Tipos públicos ---
    struct TrackingState { ... };

    // --- Construtores ---
    NomeDaClasse();

    // --- Interface pública ---
    std::vector<SearchResult> search(...) override;
    std::string name() const override { return "engine_name"; }

    // --- API pública adicional ---
    static int funcao_auxiliar(...);

private:
    // --- Membros ---
    std::unordered_set<std::string> string_pool_;

    // --- Métodos privados ---
    void metodo_interno(...);
};
```

---

## 5. S-Expression — Especificação Formal

### 5.1 Gramática

```
<sexpr>      ::= <atom> | <list>
<atom>       ::= <string> | <integer> | <keyword> | <symbol> | <bool>
<string>     ::= \" <char>* \"
<integer>    ::= [0-9]+
<keyword>    ::= : <symbol>
<symbol>     ::= [a-zA-Z_-] [a-zA-Z0-9_-]*
<bool>       ::= true | false
<list>       ::= ( <sexpr>* )
<kv-pair>    ::= <keyword> <sexpr>
<form>       ::= ( <keyword> <kv-pair>* )
```

### 5.2 Regras de Serialização

- Strings: `\"conteudo\"`. Caracteres especiais escapados: `\n`, `\t`, `\\`, `\"`.
- Keywords: prefixo `:`, sem aspas: `:total`, `:version`, `:results`.
- Integers: decimais sem zeros à esquerda: `42`, não `042`.
- Lists: `(item1 item2 item3)`.
- Nested: `(:result :file \"Main.smali\" :line 42)`.
- Pretty-print: indentação opcional para consumo humano; `--machine-sexpr` desativa.

### 5.3 Saída Padrão do Scout

```
(:scout:<tipo>
  :timestamp "2026-05-18T10:00:00Z"
  :version "<versão>"
  :total <N>
  :results (<resultados>...)
  :engine "<nome_do_motor>")
```

### 5.4 Formatos Específicos

**Search Result:**
```
(:result :file "path/Classe.smali" :line 42 :content "linha" :context "(:sexpr-contexto...)")
```

**Taint Analysis Report:**
```
(:aero-taint-report
  :start "Lcom/example;->method()V"
  :flow ((:ev :m "..." :l 5 :r "p0" :a "CALL" :t "..." :e "IMPLICIT") ...))
```

**CFG:**
```
(:cfg-report
  :method "Lcom/example;->method()V"
  :blocks ((:block :id 0 :successors (1 2) :code "..." :ipd -1) ...))
```

### 5.5 Proibições

- **Sem JSON** em qualquer ponto do pipeline de saída.
- **Sem conversão JSON → S-Expr** em qualquer camada.
- **Sem bibliotecas externas** de S-Expr. Usar `sexpr.hpp` nativa.

---

## 6. TDD — Metodologia Obrigatória

### 6.1 Ciclo Red-Green-Refactor

```
┌──────────┐     ┌──────────┐     ┌──────────┐
│   RED    │────→│  GREEN   │────→│ REFACTOR │
│ Escrever │     │ Passar   │     │ Melhorar │
│ teste    │     │ teste    │     │ código   │
│ que falha│     │          │     │          │
└──────────┘     └──────────┘     └──────────┘
```

### 6.2 Obrigações

| Obrigação | Descrição |
|-----------|-----------|
| **Teste antes do código** | Funcionalidade nova começa com teste. |
| **Teste de regressão** | Bug fix começa com teste que reproduz o bug. |
| **100% dos testes passando** | Antes de qualquer commit. |
| **Sem `#if 0` para desabilitar testes** | Testes desabilitados são deletados ou corrigidos. |
| **Cobertura mínima** | Funções novas devem ter ao menos 1 teste positivo e 1 negativo. |

### 6.3 Execução

```bash
# Todos os testes
make tests

# Teste específico
./build/test_smali_dump --gtest_filter=SmaliDumpEngine.*
./build/test_xref_search --gtest_filter=*Massive*

# Verificar sem recompilar
./build/test_all
```

### 6.4 Estrutura de Testes

```
tests/
├── test_core/               # Testes do core framework
├── test_engines/            # Testes por engine
│   ├── test_class_search.cpp
│   ├── test_content_search.cpp
│   ├── test_cfg.cpp
│   ├── test_xref_search_engine.cpp
│   └── ...
├── test_utils/              # Testes de utilitários
├── massive/                 # Testes de carga (muitos arquivos)
└── audit/                   # Testes de auditoria forense
```

---

## 7. Fluxo de Trabalho do Agente

### 7.1 Ciclo de Modificação

```
1. LER RULES.md
       │
2. ANALISAR ──→ código existente
       │           testes existentes
       │           propósito do componente
       │
3. PLANEJAR ──→ o que mudar
       │           impacto da mudança
       │           testes necessários
       │
4. MODIFICAR ──→ implementar mudança
       │
5. COMPILAR ──→ cmake --build build -j$(nproc)
       │           SEM WARNINGS. SEM ERROS.
       │
6. TESTAR ──→ make tests (ou testes específicos)
       │           TODOS VERDES.
       │
7. VALIDAR ──→ smoke test no APK real
       │           ./build/scout -p ~/wpp_decompiled --search "Lcom/whatsapp/Main;"
       │           ./build/scout -p ~/wpp_decompiled --track-var "...:p0" --track-depth 2
       │
8. COMMIT ──→ seguir formato obrigatório
       │           assinar com modelo
       │
9. CHANGELOG ──→ regenerar ou atualizar CHANGELOG.html
       │
10. FINALIZAR ──→ confirmar que tudo está em ordem
```

### 7.2 Prioridades na Tomada de Decisão

| Prioridade | Critério | Exemplos |
|------------|----------|----------|
| **P0** | Corretude da análise | Dados errados são piores que dados lentos |
| **P1** | Estabilidade | Memory leaks, crashes, UB |
| **P2** | Performance | Análise deve completar em tempo viável |
| **P3** | Legibilidade | Código deve ser compreensível por outros agentes |
| **P4** | Estilo | Convenções, formatação |

### 7.3 Validação no APK Real

O APK de teste é o WhatsApp decompilado em `~/wpp_decompiled/` (~77k arquivos smali).

Validação mínima:

```bash
# 1. Class search (deve encontrar Main.smali)
./build/scout -p ~/wpp_decompiled --search "Lcom/whatsapp/Main;" | grep -q "Main.smali"

# 2. Taint analysis (deve detectar sink)
timeout 15 ./build/scout -p ~/wpp_decompiled \
  --track-var "Lcom/whatsapp/registration/app/phonenumberentry/RegisterPhone;->onSuccess()V:p0" \
  --track-depth 2 | grep -q "SINK_LEAK"
```

---

## 8. Padrões de Projeto

### 8.1 Registry Pattern (Engines e Formatters)

Engines e formatters são registrados via `static initialization`:

```cpp
// register_engines.hpp
template <typename T>
struct EngineRegistrar {
    EngineRegistrar(const std::string& name) {
        EngineRegistry::instance().register_engine(name, []() {
            return std::make_unique<T>();
        });
    }
};
```

**Não chamar funções `create_*` manualmente.** O registro é automático.

### 8.2 Engine Pattern

Toda engine segue:

```cpp
class MinhaEngine : public ISearchEngine {
public:
    // Metadados
    std::string name() const override;
    std::string description() const override;

    // Execução
    std::vector<SearchResult> search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) override;

    // Suporte
    bool supports_config(const SearchConfig& config) const override;

    // Estatísticas
    EngineStats get_stats() const override;
};
```

### 8.3 String Pool Pattern

Para evitar alocações repetidas, strings são "internadas" em um pool:

```cpp
std::set<std::string, std::less<>> string_pool_;  // O1: heterogeneous lookup

std::string_view pool_string(std::string_view s) {
    auto it = string_pool_.find(s);
    if (it != string_pool_.end()) return *it;
    return *string_pool_.insert(std::string(s)).first;
}
```

### 8.4 Cache Pattern (Taint Analysis)

Resultados de análise são cacheados para evitar retrabalho:

```cpp
struct CacheKey {
    std::string_view method_sig;
    uint64_t reg_mask;
    uint64_t control_hash;  // C5: adicionado para contextos de controle
};

std::unordered_map<CacheKey, std::pair<std::vector<VariableEvent>, MethodSummary>, CacheKeyHash> analysis_cache_;
```

### 8.5 PointsToSet — Small Object Optimization

Usa união com small buffer (2 elementos) para evitar heap allocation na maioria dos casos:

```cpp
union {
    LocusID small[SmallSize];  // 2 elementos = sem heap
    std::vector<LocusID>* large;  // >2 elementos = heap
} storage;
```

### 8.6 Worklist Algorithm (Taint Analysis)

```
1. Construir CFG do método
2. Estado de entrada do bloco 0 = estado inicial
3. Para cada bloco na worklist:
   a. Executar função de transferência (process_method_internal)
   b. Propagar saída para sucessores (merge_states)
   c. Se estado do sucessor mudou, adicionar à worklist
4. Repetir até convergência (worklist vazia)
```

### 8.7 Análise Interprocedural

```
track_recursive:
  1. Verificar cache (CacheKey)
  2. Verificar ciclo (in_progress_methods_)
  3. Analisar método atual (worklist)
  4. Para cada CALL:
     a. Resolver target (devirtualize_call)
     b. Chamar track_recursive recursivamente
     c. Propagar retorno
  5. Armazenar no cache
  6. Retornar MethodSummary
```

---

## 9. Segurança e Restrições

### 9.1 Proibições Absolutas

| Item | Motivo |
|------|--------|
| Push sem autorização | O usuário decide quando publicar |
| `git push --force` | Destrutivo, perde histórico |
| `git reset --hard` | Destrutivo, perde trabalho |
| Modificar `build/` | Artefatos de compilação, gitignored |
| Commit de secrets | `.env`, chaves, senhas — nunca |
| Comitar sem testes | Viola TDD |
| Reintroduzir JSON no output | Filosofia Agent-First |
| Deletar código sem comentar | Dead code deve ser preservado |

### 9.2 Thread Safety

Motores podem ser chamados concorrentemente.

- Estado global deve ser `const` ou protegido por mutex.
- `string_pool_` é local por instância do engine.
- `analysis_cache_` é local por instância do engine.
- Evitar singletons mutáveis.

### 9.3 Memory Safety

- `std::string_view` só é retornado se o storage subjacente sobreviver.
- Pool de strings (`string_pool_`) garante que views inseridas permaneçam válidas durante a chamada `search()`.
- Limpar o pool entre chamadas (C6): seguro pois cada `search()` é independente.
- `PointsToSet` com `union` requer cuidado com membro ativo.

### 9.4 Undefined Behavior — Checklist

Antes de commitar, verificar:

- [ ] Nenhum acesso a `string_view` após o pool ser limpo
- [ ] Nenhuma leitura de membro inativo de `union`
- [ ] Nenhum `std::stoi` sem `try/catch` (ou melhor, use `from_chars`)
- [ ] Nenhuma comparação de `string_view` com `string` que aloca
- [ ] Nenhum iterador inválido após inserção em `unordered_map`
- [ ] Nenhuma referência pendente para contêiner realocado

---

## 10. Glossário

| Termo | Definição |
|-------|-----------|
| **Agent-First** | Projetado para consumo primário por agentes de IA, não humanos |
| **CFG** | Control Flow Graph — grafo de blocos básicos de um método |
| **CHA** | Class Hierarchy Analysis — resolução de chamadas virtuais |
| **Dalvik** | Formato de bytecode Android (.smali) |
| **FCFG** | FlowCFG — CFG com informação de fluxo de dados excepcional |
| **IPD** | Immediate Post-Dominator — nó que domina imediatamente outro no CFG |
| **LocusID** | Identificador compacto (32 bits) de um sítio de alocação |
| **PEI** | Potentially Excepting Instruction — instrução que pode lançar exceção |
| **S-Expr** | S-Expression — formato de representação de árvore de dados |
| **Sink** | Ponto de vazamento de dados (ex: `SharedPreferences.edit()`) |
| **Smali** | Representação textual do bytecode Dalvik |
| **Source** | Origem de dado sensível |
| **Taint** | Dado marcado como sensível ou contaminado |
| **TDD** | Test-Driven Development — desenvolvimento orientado a testes |
| **Worklist** | Conjunto de blocos do CFG pendentes de processamento |
| **XREF** | Cross-reference — referência cruzada entre métodos/classes |

---

## Apêndice A — Checklist do Agente

Antes de finalizar qualquer modificação, o agente deve confirmar:

```
[ ] 1. Leu RULES.md completo
[ ] 2. Entendeu o código existente (não modificou às cegas)
[ ] 3. Rodou compilação: cmake --build build -j$(nproc) — sem erros, sem warnings
[ ] 4. Rodou testes: make tests — todos verdes
[ ] 5. Rodou smoke test no APK real — class search + taint analysis
[ ] 6. Commit segue formato obrigatório com assinatura do modelo
[ ] 7. CHANGELOG.html foi atualizado (se aplicável)
[ ] 8. Nenhuma das proibições da Seção 9 foi violada
[ ] 9. Nenhum comentário foi adicionado ao código de produção
[ ] 10. O código segue as convenções de estilo da Seção 4
```

---

> *"Built by AIs, for AIs. Commits as atemporal messages. Output in S-Expr only."*
