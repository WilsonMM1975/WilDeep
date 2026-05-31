# Makefile para o WilDeep
# Compila todos os módulos e gera o executável

CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude
LDFLAGS = -pthread -lm

# Diretórios
SRC_DIR = src
OBJ_DIR = build/obj
BIN_DIR = build/bin

# Arquivos fonte (todos os .c dentro de src e subpastas)
SOURCES = $(wildcard $(SRC_DIR)/*.c) \
          $(wildcard $(SRC_DIR)/core/*.c) \
          $(wildcard $(SRC_DIR)/rede/*.c) \
          $(wildcard $(SRC_DIR)/seguranca/*.c)

# Arquivos objeto (substitui .c por .o e coloca na pasta obj)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))

# Nome do executável
TARGET = $(BIN_DIR)/wildeep

# Regra padrão (primeira)
all: $(TARGET)

# Regra para criar o executável
$(TARGET): $(OBJECTS)
	mkdir -p $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "✅ Compilação concluída! Execute: ./$(TARGET)"

# Regra para compilar arquivos .c em .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Limpeza dos arquivos compilados
clean:
	rm -rf $(OBJ_DIR)/*.o $(OBJ_DIR)/core/*.o $(OBJ_DIR)/rede/*.o $(OBJ_DIR)/seguranca/*.o
	rm -rf $(BIN_DIR)/wildeep
	@echo "🧹 Limpeza concluída!"

# Limpeza completa (remove pastas build)
distclean: clean
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "🗑️  Limpeza completa!"

# Executar o programa
run: $(TARGET)
	./$(TARGET)

# Executar com um arquivo de teste
test: $(TARGET)
	@mkdir -p dados/entrada
	@echo "Mensagem de teste do WilDeep!" > dados/entrada/teste.txt
	./$(TARGET) -e dados/entrada/teste.txt localhost

# Ajuda
help:
	@echo "Comandos disponíveis:"
	@echo "  make        - Compila o projeto"
	@echo "  make clean  - Remove os arquivos objeto"
	@echo "  make distclean - Remove tudo da compilação"
	@echo "  make run    - Executa o programa"
	@echo "  make test   - Executa com um arquivo de teste"

.PHONY: all clean distclean run test help
