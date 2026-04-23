.PHONY: all build tests clean

# Alvo padrão
all: build

# Configura o CMake (se necessário) e compila o projeto
build:
	@echo "==> Configurando e compilando o Scout++..."
	@cmake -B build
	@cmake --build build -j$$(nproc)
	@echo "==> Compilação concluída! Executável gerado em: ./build/scout"

# Compila o projeto e roda a suíte de testes unitários (TDD)
tests: build
	@echo "==> Executando testes automatizados..."
	@ctest --test-dir build --output-on-failure
	@echo "==> Todos os testes passaram com sucesso!"

# Limpa os arquivos gerados pela compilação
clean:
	@echo "==> Limpando o diretório de build..."
	@rm -rf build
	@echo "==> Limpeza concluída!"
