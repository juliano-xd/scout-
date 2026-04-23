# Scout++: Agent-First Android Reverse Engineering Framework

## 1. Visão Geral do Projeto (Project Vision)

O **Scout++** é um framework avançado de engenharia reversa e análise estática de aplicativos mobile Android (focado em bytecode Smali), reescrito integralmente em **C++26**.

**O diferencial fundamental do Scout++ é a sua arquitetura "Agent-First":**
Este projeto está sendo construído **por Inteligências Artificiais (IAs) e para ser operado por Inteligências Artificiais (IAs).**

A interface de linha de comando (CLI), a saída de dados (em formato S-Expression via `--machine-sexpr`), a modelagem de domínio e o tratamento de erros são projetados assumindo que o principal "usuário" do programa é um Agente de IA autônomo (como o próprio Gemini, Claude ou GPT). 

O objetivo é fornecer uma ferramenta de altíssima performance para que IAs possam realizar "mods" complexos, auditorias de segurança profundas e análises arquiteturais detalhadas em aplicativos Android.

**Premissa Básica:** O Scout++ assume que o aplicativo alvo **já foi completamente decompilado** pelo usuário (por exemplo, usando o `apktool`), extraindo o `AndroidManifest.xml`, recursos (`res/`) e todo o código fonte no formato Smali (`smali/`, `smali_classes2/`, etc.).

### Caso de Uso Principal (The "Why")
Um usuário humano faz uma pergunta abstrata ao seu Agente de IA: *"O que a função `doLogin` da classe `Lcom/example/AuthManager;` faz e para onde ela envia meus dados?"*
O Agente de IA, atuando de forma autônoma, deve ser capaz de invocar o Scout++, compreender suas capacidades, rodar ferramentas de Taint Analysis (rastreio de fluxo de dados), Control Flow Graph (CFG) e Cross-References (XREF) para investigar o código. Em seguida, o Agente lê a estrutura de resposta gerada pelo Scout++ e formula uma resposta precisa para o usuário, ou até mesmo gera um patch de "mod" dinâmico (via Frida) ou estático (via Smali).

---

## 2. Princípios de Design e Metodologias (Para IAs que estão codificando)

Se você é uma IA lendo este documento para continuar o desenvolvimento do Scout++, **você DEVE seguir estas diretrizes rigorosamente:**

### 2.1. Metodologia TDD (Test-Driven Development) e Integração Real
* **Desenvolvimento Guiado por Testes (Obrigatório):** TODO o novo código desenvolvido para o Scout++ deve, inexoravelmente, seguir a metodologia TDD.
* **O Fluxo:** Antes de implementar qualquer funcionalidade (como um novo parser, módulo de análise ou utilitário), você deve **primeiro** escrever um teste unitário no diretório `tests/` que valide o comportamento esperado (Red). Em seguida, implemente o código mínimo para fazer o teste passar (Green) e, por fim, otimize o código (Refactor).
* **Testes de Integração em Mundo Real:** Além dos testes unitários isolados com mocks, o sistema deve ser frequentemente testado contra uma base de código real para garantir resiliência e performance. O diretório `~/wpp_decompiled/` contém uma versão decompilada completa do WhatsApp. Testes automatizados ou scripts de validação devem rodar contra esse repositório maciço de arquivos Smali para simular o caso de uso final da ferramenta.
* **Framework:** Utilize o **GoogleTest (GTest)** (já configurado e automatizado no `CMakeLists.txt`) para criar suítes de testes claras e isoladas para cada módulo.

### 2.2. Saída de Dados Amigável para Máquinas (Machine-Readable Output)
* **Protocolo de Comunicação S-Expression (EXCLUSIVO):** O programa DEVE ser fácil de ser parseado por Agentes de IA. Por conta disso, os outputs do programa serão **APENAS e EXCLUSIVAMENTE em S-Expression** (Lisp-like). Todos os formatadores legados (JSON, Text, YAML) foram removidos e a arquitetura foi engessada neste padrão para comunicação com agentes de IA. A opção de linha de comando `--output-format` não existe mais, e a saída será estruturada nesse formato globalmente. Exemplo de saída: `(status "success") (results (class "Lcom/A;"))`.
* **Códigos de Erro Claros:** Se uma busca falhar, não jogue uma string genérica no console. Utilize exceções tipadas no C++ ou retornos semânticos (`std::optional`, `std::expected`) para que o CLI possa traduzir isso em mensagens de erro S-Expression claras. O Agente que chamou o Scout++ precisa entender *por que* falhou (ex: "Classe não encontrada", "Regex inválida") para tentar uma nova abordagem autônoma.

### 2.3. Alta Performance e C++26 Moderno
* **Zero-Copy e Mapeamento de Memória:** O gargalo de ferramentas antigas (como o projeto original em Python) era ler milhares de arquivos Smali para a memória heap (`std::string`). No C++26, utilize **Memory-Mapped Files** e `std::string_view` / `std::span` extensivamente para varrer arquivos no disco sem incorrer em custos de alocação ou cópias desnecessárias.
* **Paralelismo Agressivo:** APKs modernos possuem múltiplos diretórios Dex (`smali_classesN`). Utilize a STL moderna do C++ (`std::execution::par_unseq`, `std::jthread`, e algoritmos paralelos) para varrer arquivos, indexar referências e compilar grafos aproveitando todos os núcleos da CPU.
* **Type Safety Absoluta:** O C++ oferece tipagem forte. Modele o domínio com structs e classes bem definidas (ex: `ClassDef`, `Method`, `Field`, `Instruction`).

### 2.4. Evolução de Regex para AST (Abstract Syntax Tree)
* Ferramentas legadas dependiam de expressões regulares (Regex) em cascata para extrair informações do código. Isso quebra facilmente com obfuscações.
* **Objetivo Arquitetural:** O Scout++ deve caminhar para uma análise estrutural (Tokenization / Lexing). A ferramenta deve "entender" que uma linha Smali é composta por um Opcode, Registradores e Referências, e criar uma árvore de sintaxe real em memória, não apenas buscar um padrão textual.

---

## 3. Modularidade e Estrutura de Diretórios

O código deve manter uma organização limpa, escalável e separada por responsabilidades:

* `include/cli/` e `src/cli/`: Interface de Linha de Comando, Parse de argumentos (já centralizado em `parser.hpp`).
* `include/core/` e `src/core/`: Scanner de disco, IO de alta performance, Parsing estrutural do Smali.
* `include/domain/`: As estruturas de dados que representam as entidades do Android (`Class`, `Method`, `Field`).
* `include/analysis/` e `src/analysis/`: A "Inteligência" (Taint Analysis, XREF, Control Flow Graph, ObfuscationDetector).
* `include/utils/` e `src/utils/`: Helpers como travessia de FileSystem e estruturas genéricas de C++.
* `tests/`: Suítes de testes em GoogleTest para validação da metodologia TDD.

---

## 4. Capacidades Core que Devem ser Desenvolvidas

Para atingir a excelência na análise reversa mobile, as seguintes ferramentas devem ser priorizadas e codificadas no framework:

1. **Recon & Manifest:** Parsing rápido do `AndroidManifest.xml` para extrair componentes exportados, permissões perigosas e flags de debug.
2. **Scanner Estático Avançado:** Buscas paralelas super-rápidas por strings, inteiros, padrões de criptografia (AES, RSA) e APIs vulneráveis.
3. **Cross-References (XREF):** O coração da análise. Indexar rapidamente o código para saber exatamente quem chama quem (`callers`) e quem é chamado por quem (`callees`) em tempo O(1).
4. **Control Flow Graph (CFG) & Taint Analysis:** Compreender o fluxo de blocos básicos (Basic Blocks) dentro de um método para rastrear o fluxo de variáveis sensíveis de um "Source" (ex: GPS, contatos) para um "Sink" (ex: internet, logs).
5. **Resource Mapping:** Mapear IDs hexadecimais de recursos UI (`0x7f0b0000`) encontrados no bytecode para seus nomes reais e *event handlers*.
6. **Code Modification (Patching/Hooking):** Inserção algorítmica de código Smali no início/fim de métodos existentes de forma transacional, permitindo que a IA aplique "mods" autônomos sem corromper o APK.