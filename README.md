# Scout++ - Agent-First Forensic Framework

Scout++ é uma ferramenta de análise estática e forense focada no ecossistema Android (arquivos Dalvik/Smali). Projetada com foco em performance e extensibilidade, utiliza S-Expressions nativamente para representar resultados de busca, Árvores de Sintaxe Abstrata (AST) e Fluxos de Controle (CFG), tornando-se altamente compatível com integração via IA e processamento automatizado.

## 🚀 Principais Funcionalidades

- **Busca de Classes e Conteúdo:** Mecanismos de busca ultrarrápidos para encontrar classes por nome (suporte a notação Dalvik e substrings) e buscar conteúdo textual e padrões.
- **Análise de Referências Cruzadas (XREF):** Rastreie chamadores (callers) e chamados (callees) de métodos, bem como o uso de campos na aplicação.
- **Geração de Grafo de Fluxo de Controle (CFG):** Extração detalhada do CFG de métodos Smali, mapeando os blocos básicos e suas ramificações.
- **Dump de AST Smali:** Transformação de arquivos `.smali` em S-Expressions para facilitar a análise automatizada das estruturas de classe, campos, métodos e instruções.
- **Mapeamento de UI e Recursos:** Ferramentas para analisar uso de recursos e elementos de interface.
- **Taint Analysis (Rastreamento de Variáveis):** Rastreie o ciclo de vida e a propagação de variáveis específicas através dos métodos da aplicação.

## 📋 Pré-requisitos

Para compilar e rodar o Scout++, você precisará das seguintes ferramentas:

- **CMake** (versão 3.20 ou superior)
- **Compilador C++** com suporte ao padrão **C++26** (ex: GCC 14+, Clang 17+)
- **Make** ou Ninja (para o processo de build)
- **Git** (para baixar dependências como Google Test e Google Benchmark)

As bibliotecas externas (`nlohmann_json`, `googletest`, `googlebenchmark`) são gerenciadas automaticamente pelo CMake através do `FetchContent`.

## 🛠️ Como Compilar e Instalar

O projeto utiliza um `Makefile` na raiz para simplificar os comandos do CMake. 

Para compilar todo o projeto, execute:

```bash
make build
# ou simplesmente:
make
```

Isso irá criar o diretório `build/`, baixar as dependências necessárias, configurar o projeto e compilar a biblioteca estática (`scout_lib`) e o executável principal (`scout`).

O binário compilado estará disponível em `./build/scout`.

## 🧪 Como Executar os Testes

O Scout++ adota a filosofia de Test-Driven Development (TDD) com extensa cobertura de testes usando o **Google Test**. Os testes estão divididos entre unitários, casos extremos (edge cases) e suítes massivas de validação.

Para compilar e rodar todos os testes automatizados, execute:

```bash
make tests
```

Se precisar executar um teste específico, você pode usar o binário de teste diretamente e aplicar o filtro do Google Test:

```bash
./build/test_smali_dump --gtest_filter=SmaliDumpEngine.*
```

### Benchmarks

Para avaliar a performance dos motores de busca e formatação, utilize o Google Benchmark:

```bash
make benchmark
```

## 💻 Como Usar (CLI)

O executável principal `./build/scout` oferece uma interface de linha de comando (CLI) rica e modular.

**Exemplos básicos de uso:**

1. **Buscar uma classe específica (Notação Dalvik):**
   ```bash
   ./build/scout -p /caminho/para/apk_descompactado --search "Lcom/example/MainActivity;"
   ```

2. **Analisar Referências Cruzadas (XREF - Quem chama um método):**
   ```bash
   ./build/scout -p /caminho/para/apk_descompactado --xref-callers "Lcom/example/Auth;->login()Z"
   ```

3. **Gerar CFG de um método:**
   ```bash
   ./build/scout -p /caminho/para/apk_descompactado --cfg "Lcom/example/Utils;->hash(Ljava/lang/String;)Ljava/lang/String;"
   ```

4. **Dump de AST Smali:**
   ```bash
   ./build/scout -p /caminho/para/apk_descompactado --dump-ast "Lcom/example/Config;"
   ```

Para ver todas as opções disponíveis, execute:
```bash
./build/scout --help
```

## 🏗️ Estritetura do Projeto

- `/include` - Cabeçalhos (Headers) do projeto, definindo a arquitetura em módulos (`core`, `engines`, `utils`, `cli`, `formatters`).
- `/src` - Implementação (.cpp) dos motores e funções centrais.
- `/tests` - Suítes de testes automatizados organizados por escopo (unitário, massivo, auditoria).

## 📄 Formato de Saída (S-Expression)

O Scout++ é "Agent-First" e utiliza S-Expressions como sua estrutura de saída padrão, garantindo previsibilidade e facilidade de parsing para ferramentas de inteligência artificial ou scripts de terceiros. Use a flag `--machine-sexpr` (ou `-x`) para saídas otimizadas e sem formatação estética (pretty-print desativado).
