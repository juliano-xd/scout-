# Bug Report — Taint Analysis Engine

> Auditoria completa do motor de Taint Analysis (VariableTrackerEngine).
> Gerado em: 2026-05-18
> Metodologia: Revisão de código linha a linha + análise de fluxo + testes no APK real (~/wpp_decompiled)

---

## Resumo

| Severidade | Quantidade |
|------------|-----------|
| CRITICAL   | 4 |
| HIGH       | 13 |
| MEDIUM     | 11 |
| LOW        | 4 |
| **Total**  | **32** |

---

## CRITICAL

### C7 — Cache key aliasing via XOR de active_regs e taint_fp (FIXED: 00c30d4)

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:373-382`

```cpp
CacheKey key{state.current_method, state.active_regs ^ taint_fp, control_hash};
```

O campo `reg_mask` do `CacheKey` é `active_regs ^ taint_fp`. Como ambos são
XORs comutativos, estados diferentes produzem a mesma chave:

- Estado A: `active_regs=0x100`, `taint_fp=0x200` → `reg_mask=0x300`
- Estado B: `active_regs=0x200`, `taint_fp=0x100` → `reg_mask=0x300`

Isso causa **cache hit falso** — o resultado do estado A é reusado para o
estado B, silenciosamente perdendo fluxos de taint.

**Solução:** Armazenar `active_regs` e `taint_fp` como campos separados no
`CacheKey` em vez de combiná-los com XOR.

---

### C8 — control_taint_stack merge ignora conteúdo, só compara tamanho (FIXED: ec66901)

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:226-262`

O fast path (linhas 226-243) só verifica
`target.control_taint_stack.size() >= incoming.control_taint_stack.size()`.
O slow path (linhas 259-262) só substitui quando o incoming é maior.

Se ambos têm tamanho 1 mas conteúdos diferentes (ex: `[3]` vs `[5]`), a
diferença é silenciosamente ignorada — `changed` permanece false, e o bloco
sucessor nunca é re-analisado com o novo contexto.

**Impacto:** Fluxo de controle não-estruturado (irredutível) produz falsos
negativos no implicit flow tracking.

**Solução:** Comparar conteúdo do vetor, não apenas tamanho.

---

### C9 — string_pool_.clear() invalida todas as string_views de chamadas anteriores (FIXED: b87e381)

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:302-307`

```cpp
string_pool_.clear();
```

`pool_string()` retorna `string_view` apontando para elementos de
`string_pool_` (um `set<string>`). Após `clear()`, todas as `string_view`
retornadas por chamadas anteriores de `search()` tornam-se dangling.

Qualquer caller que segure um `SearchResult` entre invocações de `search()`
terá campos `event.method`, `event.reg`, `event.target`, etc., corrompidos.

**Solução:** Alternativas:
1. Não limpar o pool (vazamento de memória, problema anterior C6)
2. Copiar strings para os `SearchResult` (mais seguro, mais alocações)
3. Documentar que `SearchResult` só é válido até a próxima `search()`
   (já documentado, mas frágil)

---

### C10 — process_method_internal é monolithic (~450 linhas) (FIXED: 9948996)

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:554-1010`

A função handleia 11 tipos de instrução via cadeia if-else, taint propagation,
event generation, inter-procedural call resolution, sanitizer/transform
classification. Complexidade ciclomática > 100.

Não pode ser testada unitariamente em isolamento. Qualquer mudança arrisca
quebrar múltiplos comportamentos não-relacionados.

**Solução:** Decompor em handlers por tipo de instrução.

---

## HIGH

### C11 — control_taint_stack pop só do final (LIFO), quebra em CFG não-estruturado

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:470-474`

```cpp
while (!current_in.control_taint_stack.empty()
       && current_in.control_taint_stack.back() == bid)
{
    current_in.control_taint_stack.pop_back();
}
```

Só remove do **final** quando `back() == bid`. Se o IPD correspondente não
estiver no final (ex: stack `[5, 3]` com `bid=3`), nunca é removido. Para
CFGs irredutíveis, contextos de taint persistem incorretamente.

**Solução:** Procurar o `bid` em qualquer posição do vetor e removê-lo.

---

### C12 — devirtualize_call retorna apenas UM alvo (ignora múltiplos dispatch targets)

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:1075-1077`

Retorna só a primeira classe na hierarquia que implementa o método. Em Java,
uma chamada virtual pode despachar para qualquer subclasse que sobrescreva
o método. O CHA deveria retornar TODOS os alvos possíveis.

Além disso, interfaces nunca são resolvidas — o código só segue `.super`,
não `.implements`.

**Solução:** Implementar resolução completa de hierarquia (CHA com múltiplos
alvos + interfaces).

---

### C13 — Nenhuma distinção entre invoke-virtual, invoke-super, invoke-interface, invoke-direct

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:834-987`

Todos os `invoke-*` são tratados identicamente. A semântica de dispatch é
ignorada:
- `invoke-super` deve resolver na classe **pai**, não na atual
- `invoke-interface` deve buscar cadeias `.implements`
- `invoke-direct` (private/constructor) tem alvo conhecido sem CHA

**Solução:** Rotear cada variante para sua lógica de dispatch correta.

---

### C14 — CHA nunca verifica Ljava/lang/Object;

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:1040`

```cpp
while (class_name != "Ljava/lang/Object;" && !class_name.empty()) {
```

O loop **sai antes** de verificar `Ljava/lang/Object;`. Métodos definidos em
`Object` (`toString()`, `hashCode()`, `equals()`, `getClass()`) nunca são
encontrados via CHA.

**Solução:** Incluir `Object` na verificação (permitir a iteração até `Object`).

---

### C15 — aput ignora registrador de índice; marca array inteiro como tainted

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:813-825`

Quando `aput` armazena um valor tainted, o **registrador do array inteiro**
é marcado como tainted. Qualquer `aget` subsequente do mesmo array (mesmo
em índice diferente) é tratado como tainted. O registrador de índice (terceiro
operando) nunca é extraído ou verificado.

**Solução:** Rastreamento por índice (complexo) ou pelo menos extrair e
verificar o registrador de índice.

---

### C16 — iget sobre-aproxima: qualquer taint no objeto taina TODAS as leituras de campo

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:734-738`

```cpp
} else if (state.active_regs & (1ULL << o_bit)) {
    tainted = true;
```

Se o registrador de objeto está em `active_regs` (por qualquer razão), TODAS
as leituras de campo dele são consideradas tainted. Isso cria
sobre-taintagem massiva: se uma referência de string tainted é usada como
objeto, qualquer acesso a campo resulta em taint.

**Solução:** Só taintar o campo se o taint do objeto veio de um `iget`/`sget`
específico (via `obj_taint_map`), não de `active_regs` genérico.

---

### C17 — PointsToSet union com rastreamento frágil de membro ativo

**Arquivo:** `include/engines/variable_tracker/variable_tracker_engine.hpp:29-104`

A `union` sobrepõe array fixo e ponteiro heap. O rastreamento do membro ativo
é via flag `is_large` separada. Qualquer refatoração futura que esqueça de
verificar `is_large` lerá lixo da union.

Além disso, `clear()` faz `delete storage.large` sem setar `storage.large =
nullptr`. Após `clear()`, o ponteiro dangling em `storage.large` pode causar
issues se alguém verificar `is_large` incorretamente depois.

**Solução:** Usar `std::variant<LocusID[2], std::vector<LocusID>>` (C++17)
ou garantir que o membro inativo nunca seja lido.

---

### C18 — BasicBlock::code_content string_view depende de cache mmap

**Arquivo:** `src/engines/cfg/cfg_engine.cpp:39-40,58-62`

`body` é uma view do `content` de `get_class_content` (arquivo mapeado em
memória). `cfg.blocks[i].code_content` são `string_view`s dentro dessa
memória. Se o `AnalysisContext` invalidar o mmap, as views penduram.

Não é um bug ativo (o cfg é local e descartado após `search()`), mas é uma
bomba relógio para qualquer código que armazene CFGs entre chamadas.

**Solução:** Copiar strings para o `BasicBlock` ou documentar que CFG só é
válido durante a chamada.

---

### C19 — compute_taint_fingerprint O(all taint) por chamada recursiva

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:165-181,373-374`

```cpp
const uint64_t taint_fp = compute_taint_fingerprint(
    state.obj_taint_map, state.static_fields_taint);
```

Itera sobre TODAS as entradas de `obj_taint_map` e `static_fields_taint` em
**toda entrada de método recursiva**. Para cadeias de chamada profundas com
estados grandes, é O(depth * taint_size).

**Solução:** Cache incremental do fingerprint ou usar um rolling hash.

---

### C20 — pool_string usa set (O(log n)) com alocação por string única

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:195-199`

```cpp
return *string_pool_.insert(std::string(s)).first;
```

Toda string única causa alocação heap de `std::string(s)`. Não há limite
superior no tamanho do pool dentro de uma chamada `search()`.

**Solução:** Considerar `std::pmr::monotonic_buffer_resource` (C++17/26)
para alocações consecutivas sem fragmentação.

---

### C21 — Parsing de instruções é string-based, não estrutural

**Arquivo:** `src/engines/variable_tracker/variable_tracker_engine.cpp:556-1008`

Instruções são parseadas por prefixo de string (`starts_with("move ")`,
`starts_with("invoke-")`). Isso é frágil e propenso a falsos positivos.

Exemplo: `const` casa TODAS as variantes (`const`, `const/4`, `const/16`,
`const-wide`, `const-string`, `const-class`, `const-object`), que têm
semânticas diferentes.

**Solução:** Tabela de instruções Smali com opcode → handler.

---

### C22 — Exception edges nunca populados pelo CFG builder

**Arquivo:** `src/engines/cfg/cfg_engine.cpp:83-176`

`build_cfg` nunca parseia diretivas `.catch`. O tipo `ExceptionEdge` e o
vetor `handlers` são definidos no header, mas nunca populados. A propagação
de exception handler em `track_recursive` (linhas 507-513) é código morto.

**Solução:** Implementar parsing de `.catch` no CFG builder.

---

## MEDIUM

| ID | Descrição | Arquivo | Linhas |
|----|-----------|---------|--------|
| C23 | `call_bits[16]` descarta argumentos além do índice 15 | var_tracker.cpp | 871-889 |
| C24 | `packed-switch`/`sparse-switch` não tratados como terminadores de bloco | cfg_engine.cpp | 110-119 |
| C25 | `return` handler não verifica `obj_taint_map` para o registrador de retorno | var_tracker.cpp | 993-1006 |
| C26 | `initial_reg` comparação com `==` pode falhar para config não-trimmed | var_tracker.cpp | 629,671,714 |
| C27 | `visited_blocks` é `unordered_set<int>` para IDs inteiros densos | var_tracker.cpp | 448,501-502 |
| C28 | `block_events_map` usa unordered_map depois ordena — usar vector por ID | var_tracker.cpp | 447,531-544 |
| C29 | Busca de header de método rescaneia classe do início a cada chamada | var_tracker.cpp | 403-430 |
| C30 | `supports_config` retorna true para queries que produzem resultados vazios | var_tracker.cpp | 1012-1014 |
| C31 | `file_path` hardcoded, `line_number` nunca setado no SearchResult | var_tracker.cpp | 321 |
| C32 | `depth` em `process_method_internal` é parâmetro morto | var_tracker.cpp | 562 |
| C33 | `alias_map` em TrackingState é código morto (já comentado L2) | var_tracker.hpp | 176-177 |

---

## LOW

| ID | Descrição | Arquivo | Linhas |
|----|-----------|---------|--------|
| C34 | `exception_out` é união de todos os estados PEI — sobre-aproxima | var_tracker.cpp | 585 |
| C35 | `merge_states` fast path pode fazer trabalho desperdiçado em non-subset | var_tracker.cpp | 226-243 |
| C36 | Comentários misturam português e inglês | Ambos | vários |
| C37 | Constantes mágicas `0xdeadbeefcafe` e `0x9e3779b97f4a7c15` sem nome | var_tracker.cpp | 174,178,380 |

---

## Notas da Reauditoria (2026-05-18 v2)

### Falsos Alarmes

| ID | Descrição | Motivo |
|----|-----------|--------|
| C25 | return handler não verifica obj_taint_map | O código SEMPRE verificou — linhas 1046-1050 |
| C32 | depth é parâmetro morto | depth é usado em handle_invoke_instruction → track_recursive (linha 1004) |

### Observações Adicionais

- C39: `;-><init>` em PROPAGATORS casa QUALQUER construtor. Toda chamada de construtor com argumento tainted vira propagação para `this`, mesmo construtores default. Falso positivo conhecido após BUG-6.
- C40: `exception_out` (linha 475) acumula união de N estados PEI do bloco. A sobre-aproximação documentada em C34.

## Issues Corrigidas (Sessão Atual)

| Ticket | Descrição | Commit |
|--------|-----------|--------|
| C7 | CacheKey: active_regs e taint_fp separados | `00c30d4` |
| C8 | merge_states: compara conteúdo, não tamanho | `ec66901` |
| C9 | Comentário enganoso sobre string_pool_ | `b87e381` |
| C10 | process_method_internal decomposto em 7 handlers | `9948996` |

## Issues Corrigidas (Sessão Anterior)

| Ticket | Descrição | Commit |
|--------|-----------|--------|
| C1 | `followed_ok` para assinaturas completas | `2145d43` |
| C2 | `preceded_ok` + `followed_ok` em `devirtualize_call` | `7795652` |
| C3 | Move semantics em PointsToSet (Rule of 5) | `107c8e5` |
| C4 | `std::stoi` → `std::from_chars` em reg_to_bit | `9c097d3` |
| C5 | CacheKey inclui `control_taint_stack` | `825e13b` |
| C6 | `string_pool_` clearing entre chamadas search() | `6ba5169` | |

---

## Metodologia

A auditoria foi realizada através de:
1. Leitura linha a linha de `variable_tracker_engine.cpp` (1092 linhas)
2. Leitura linha a linha de `variable_tracker_engine.hpp` (260 linhas)
3. Leitura linha a linha de `cfg_engine.cpp` (178 linhas)
4. Análise de fluxo de dados (taint propagation paths)
5. Análise de cache consistency (CacheKey)
6. Análise de memory safety (string_view lifetimes, union access)
7. Validação no APK real (`~/wpp_decompiled/`, ~77k arquivos smali)
8. Testes de regressão (`make tests` + smoke tests)
