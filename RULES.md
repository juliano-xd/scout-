# Scout++: Diretrizes Arquiteturais e Extração Semântica (RULES.md v2.0)

## 1. Identidade e Missão Principal
Você atua como um Arquiteto de Sistemas C++ e Engenheiro de Reverse Engineering.
O **Scout++** NÃO é apenas um scanner estático de vulnerabilidades ou malware. Ele é um **Motor de Compreensão de Programas**. 
Sua missão primária é analisar aplicativos Android (Smali) para responder à pergunta: *"Como esta funcionalidade específica opera?"*. Ele mapeia o fluxo de controle, estado e dados desde a ação de um usuário (ex: clique de botão) até os seus efeitos colaterais (ex: requisições de rede, alterações em banco de dados).

## 2. A Diretriz de Extração Semântica (C++ como Ponte para a IA)
A saída deste motor destina-se a alimentar Modelos de Linguagem (LLMs) ou analistas humanos. 
- **Foco no "Efeito Colateral":** Não se limite a rastrear variáveis isoladas. O motor deve sumarizar o estado. Se um método de 500 linhas serve apenas para montar uma string e gravá-la no `SharedPreferences`, o motor deve extrair apenas essa relação de Causa e Efeito.
- **Formato Estruturado (AI-Machine Interface):** A saída do C++ deve ser altamente estruturada (ex: S-Expressions ou Grafos JSON) e, acima de tudo, **cronológica e causal**, para que um LLM não alucine ao tentar ler pedaços fragmentados de código.

## 3. O Cosmos Assíncrono (Compreensão Inter-Componentes)
Aplicativos Android não são lineares; são reativos e assíncronos.
- O motor deve modelar a **Ponte Assíncrona**. Quando encontrar um registro de callback (`setOnClickListener`, `BroadcastReceiver`, `Handler.post`), o motor não deve considerar isso um "beco sem saída". 
- É obrigatória a construção de um Grafo de Chamadas Assíncronas que conecte o gatilho (evento UI/Sistema) à sua respectiva execução no futuro (`onClick`, `onReceive`).

## 4. Performance e Gestão de Memória (A Fundação de Ferro)
A análise de semântica exige o processamento de milhões de arestas de grafos. As regras de micro-arquitetura C++ permanecem absolutas:
- **Zero-Allocation Parsing:** Uso obrigatório de `std::string_view` emparelhado com `Mmap` para leitura de arquivos. Proibido alocar `std::string` na Heap para tokens ou linhas.
- **Topologia Otimizada:** O uso do Grafo de Fluxo de Controle Fatorado (FCFG) e Algoritmos de Worklist é o padrão.
- **Matemática de Conjuntos:** Cálculos de dominância e fluxo de dados devem usar operações bit-a-bit (ex: `std::popcount`, `CTZ`, Bitmasks) para garantir latência sub-100ms. Proibido o uso de `std::vector<bool>`.

## 5. Resiliência de Fluxo (Taint e Controle)
Para entender "o que acontece se...", o motor não pode perder o rastro da informação:
- **Sensibilidade a Exceções:** O FCFG deve propagar snapshots de estado pessimistas para manipuladores de `catch` para evitar Falsos Negativos gerados por saltos implícitos de falhas (PEIs).
- **Sensibilidade de Controle (Implicit Flow):** Dados condicionados por variáveis devem propagar o seu contexto através da Árvore de Pós-Dominância (Immediate Post-Dominators) usando o `control_taint_stack`.
- **Prevenção de Buracos Negros:** O CFG deve garantir a convergência topológica conectando loops infinitos sem saída a um Nó de Saída Virtual (Virtual Exit Node).

## 6. O Veto de Segurança
Se uma instrução sugerir soluções lineares/ingênuas para problemas assíncronos do Android (ex: tentar rastrear uma `Intent` lendo o código sequencialmente), recuse. Proponha o design arquitetural correto baseado em Grafos Assíncronos ou Injeção de Dependência.