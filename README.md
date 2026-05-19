# Scout++ — Agent-First Forensic Framework

Framework forense para analise estatica de aplicativos Android (Dalvik/Smali).
Toda saida utiliza **S-Expression** nativamente — sem JSON, XML ou intermediarios.
Construido em C++26 com zero-copy mmap e paralelismo <code>par_unseq</code>.

11 engines registradas | 30+ flags CLI | 18 taint sinks | 5 sanitizers | 8 transforms

---

<h2>Funcionalidades</h2>

<h3>Busca e Navegacao</h3>

<table>
<tr><th>Flag</th><th>Descricao</th></tr>
<tr><td><code>--search</code> / <code>-f</code></td><td>Busca por classe (notacao <code>L...;</code>) ou substring. Engine auto-detectada</td></tr>
<tr><td><code>--search-type</code></td><td>Modo: <code>class</code>, <code>string</code>, <code>regex</code>, <code>integer</code></td></tr>
<tr><td><code>--search-max</code></td><td>Limite maximo de resultados (default: 1000)</td></tr>
<tr><td><code>--case-sensitive</code></td><td>Case-sensitive em content search</td></tr>
<tr><td><code>--include-dir</code> / <code>--exclude-dir</code></td><td>Filtro por diretorios</td></tr>
<tr><td><code>--inspect-class</code> / <code>-i</code></td><td>DNA da classe: superclasse, interfaces, fields, metodos, risk analysis</td></tr>
<tr><td><code>--manifest</code> / <code>-m</code></td><td>Analise do <code>AndroidManifest.xml</code>: package, permissions, components, security alerts</td></tr>
</table>

<h3>Cross-Reference (XREF)</h3>

<table>
<tr><th>Flag</th><th>Descricao</th></tr>
<tr><td><code>--xref</code> / <code>-X</code></td><td>Cross-reference bidirecional (callers + callees de um metodo)</td></tr>
<tr><td><code>--xref-callers</code></td><td>Apenas quem chama o metodo alvo</td></tr>
<tr><td><code>--xref-callees</code></td><td>Apenas quem e chamado pelo metodo alvo</td></tr>
<tr><td><code>--xref-fields</code></td><td>Usos de um campo (leitura/escrita)</td></tr>
<tr><td><code>--xref-direction</code></td><td>Direcao: <code>callers</code>, <code>callees</code>, <code>both</code></td></tr>
<tr><td><code>--xref-depth</code> / <code>-d</code></td><td>Profundidade recursiva da busca</td></tr>
<tr><td><code>--xref-include-system</code> / <code>-S</code></td><td>Incluir classes do framework Android</td></tr>
</table>

<h3>CFG e AST</h3>

<table>
<tr><th>Flag</th><th>Descricao</th></tr>
<tr><td><code>--cfg</code> / <code>-C</code></td><td>Geracao do CFG (Control Flow Graph) de um metodo com exception edges e dominator analysis</td></tr>
<tr><td><code>--dump-ast</code></td><td>Dump da AST Smali como S-Expression estruturada</td></tr>
<tr><td><code>--translate</code></td><td>Traducao de Smali para pseudo-codigo estilo C</td></tr>
</table>

<h3>Taint Analysis</h3>

<table>
<tr><th>Flag</th><th>Descricao</th></tr>
<tr><td><code>--track-var</code> / <code>-t</code></td><td>Rastreio inter-procedural de taint: <code>Classe;->method:reg</code></td></tr>
<tr><td><code>--track-var-name</code></td><td>Nome da variavel/registrador alvo (ex: <code>v0</code>, <code>p1</code>)</td></tr>
<tr><td><code>--track-depth</code></td><td>Profundidade maxima de recursao inter-procedural</td></tr>
</table>

<p>18 sinks (Log, I/O, Network, DB, Runtime.exec, SMS, SharedPreferences, WebView, IPC, native loading).<br>
5 sanitizers (MessageDigest, Cipher, CRC32, hashCode, escapeHtml).<br>
8 transforms (Base64, URLEncoder, Integer.toHexString...).<br>
6 propagators (StringBuilder.append, Map.put, construtores).<br>
Fluxo implicito via control_taint_stack com IPD. CHA (Class Hierarchy Analysis) para dispatch virtual.<br>
Cache memoizante por CacheKey {method_sig, reg_mask, control_hash}.</p>

<h3>Recursos e UI</h3>

<table>
<tr><th>Flag</th><th>Descricao</th></tr>
<tr><td><code>--resource-map</code> / <code>-R</code></td><td>Mapeamento de todos os resource IDs (hex → nome simbolico)</td></tr>
<tr><td><code>--find-resource</code></td><td>Busca uso de um resource ID ou nome especifico</td></tr>
<tr><td><code>--ui-mapper</code> / <code>-u</code></td><td>Ponte XML de layout → codigo Smali (event handlers)</td></tr>
</table>

<h3>Engenharia Reversa</h3>

<table>
<tr><th>Flag</th><th>Descricao</th></tr>
<tr><td><code>--deobf-strings</code> / <code>-D</code></td><td>Scan de strings ofuscadas (Base64, alta entropia)</td></tr>
<tr><td><code>--detect-obfuscation</code></td><td>Detecao estrutural de ofuscacao em classes</td></tr>
<tr><td><code>--obf-types</code></td><td>Tipos de ofuscacao a detectar</td></tr>
</table>

<h3>Infraestrutura</h3>

<table>
<tr><th>Flag</th><th>Descricao</th></tr>
<tr><td><code>--path</code> / <code>-p</code></td><td>Diretorio do APK decompilado (obrigatorio)</td></tr>
<tr><td><code>--machine-sexpr</code> / <code>-x</code></td><td>S-Expression compacta (sem pretty-print)</td></tr>
<tr><td><code>--verbose</code> / <code>-v</code></td><td>Log detalhado (engines disponiveis, diretorio atual)</td></tr>
<tr><td><code>--ai-help</code></td><td>Documentacao estruturada em S-Expression para LLMs</td></tr>
<tr><td><code>--help</code></td><td>Lista completa de flags</td></tr>
</table>

<p><strong>Modulos pendentes</strong> (esboco CLI implementado, engine nao implementada):
<code>--scan</code>, <code>--hook</code>, <code>--frida</code> — retornam <code>(:scout:pending :status &quot;not_implemented&quot;)</code>.</p>

---

<h2>Formato de Saida (S-Expression)</h2>

<p>Toda saida do Scout++ segue a gramatica S-Expression:</p>

<pre><code>(:scout:&lt;tipo&gt;
  :timestamp "2026-05-19T10:00:00Z"
  :version "1.1"
  :total &lt;N&gt;
  :results (&lt;resultados&gt;...)
  :engine "&lt;nome_do_motor&gt;")
</code></pre>

<h3>Formatos Especificos</h3>

<p><strong>Search Result:</strong></p>

<pre><code>(:result :file "path/Classe.smali" :line 42 :content "linha" :context "(:sexpr-contexto...)")
</code></pre>

<p><strong>Taint Analysis:</strong></p>

<pre><code>(:aero-taint-report
  :start "Lcom/example;->method()V"
  :flow ((:ev :m "..." :l 5 :r "p0" :a "CALL" :t "..." :e "IMPLICIT") ...))
</code></pre>

<p><strong>CFG:</strong></p>

<pre><code>(:cfg-report
  :method "Lcom/example;->method()V"
  :blocks ((:block :id 0 :successors (1 2) :code "..." :ipd -1) ...))
</code></pre>

<p><strong>Progresso (stderr):</strong></p>

<pre><code>(:|scout:progress| :engine "xref" :status "running" :timestamp "2026-05-19T10:00:00Z" :query "Lcom/...;")
(:|scout:progress| :engine "xref" :status "done" :results 42 :elapsed_ms 1234)
</code></pre>

<p>Progresso em stderr, resultados em stdout. Em TTY o <code>\r</code> sobrescreve a linha.</p>

---

<h2>Uso</h2>

<pre><code># Busca de classe
./build/scout -p ~/app_decompiled --search "Lcom/example/MainActivity;"

# Busca textual com regex
./build/scout -p ~/app_decompiled -f "api_key|token" --search-type regex

# Cross-reference (quem chama onCreate)
./build/scout -p ~/app_decompiled --xref-callers "Lcom/example/Main;->onCreate(Landroid/os/Bundle;)V"

# Referencias a campo
./build/scout -p ~/app_decompiled --xref-fields "Lcom/example/Config;->API_URL:Ljava/lang/String;"

# CFG com exception edges
./build/scout -p ~/app_decompiled -C "Lcom/example/Auth;->login()Z"

# Taint analysis
./build/scout -p ~/app_decompiled --track-var "Lcom/example/Auth;->getToken()Ljava/lang/String;:v0" --track-depth 3

# AST da classe
./build/scout -p ~/app_decompiled --dump-ast "Lcom/example/Main;"

# Traducao para pseudo-codigo
./build/scout -p ~/app_decompiled --translate "Lcom/example/Crypto;->decrypt([B)[B"

# DNA da classe
./build/scout -p ~/app_decompiled -i "Lcom/example/Main;"

# Manifest analysis
./build/scout -p ~/app_decompiled -m

# Resource map
./build/scout -p ~/app_decompiled -R | head -20

# UI mapper (encontrar handlers de um recurso)
./build/scout -p ~/app_decompiled -u "0x7f0a0042"

# Deobfuscation scan
./build/scout -p ~/app_decompiled -D

# Multiplas analises em um comando
./build/scout -p ~/app_decompiled -i "Lcom/example/Main;" -f "onCreate" --search-max 5

# Saida S-Expression compacta (para parser)
./build/scout -p ~/app_decompiled --search "Lcom/example/Main;" -x
</code></pre>

---

<h2>Compilacao</h2>

<p><strong>Pre-requisitos:</strong> CMake 3.20+, compilador C++26 (GCC 14+, Clang 17+), Make ou Ninja.</p>

<pre><code># Compilar
make

# Compilar e rodar testes
make tests

# Benchmarks de performance
make benchmark

# Limpar build
make clean
</code></pre>

<p>Dependencias externas (Google Test, Google Benchmark) baixadas automaticamente via <code>FetchContent</code>.</p>

---

<h2>Testes</h2>

<p>Suite Google Test com cobertura por engine e por utilitario.</p>

<pre><code>make tests                    # Todos os testes
./build/test_progress_reporter # Teste especifico
</code></pre>

---

<h2>Arquitetura</h2>

<pre><code>CLI       Parser (argc/argv) → ScoutConfig
  |
Engine    EngineRegistry → ISearchEngine (11 engines)
  |
Format    FormatterRegistry → SExprFormatter (S-Expression)
  |
Core      AnalysisContext (mmap cache, index)
          Scanner (parallel FS, atomic abort)
          SmaliParser (zero-alloc method extraction)
  |
Utils     SExpr AST → StringUtils → MmapFile → Filesystem → ProgressReporter
</code></pre>

<p>11 engines registradas: <code>class</code>, <code>content</code>, <code>xref</code>, <code>resource_map</code>, <code>cfg</code>, <code>manifest</code>,
<code>class_inspector</code>, <code>ui_mapper</code>, <code>deobf</code>, <code>taint_analysis</code>, <code>smali_dump</code>.</p>

---

<h2>Estrutura do Projeto</h2>

<pre><code>include/         Headers publicos (core, engines, utils, cli, formatters)
src/             Implementacao (mapeamento 1:1 com headers)
tests/           Testes Google Test
scripts/         Ferramentas auxiliares (taint_report.py)
build/           Artefatos de compilacao (gitignored)
</code></pre>

---

<h2>Licenca</h2>

<p>Scout++ — Agent-First Forensic Toolkit.<br>
C++26 · S-Expression Native · Zero-Copy · Zero-Allocation.</p>
