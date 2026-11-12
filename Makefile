CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc
LDFLAGS = 

TARGET = ghost
SRCS = src/main.c src/error.c src/ast.c src/symtab.c src/semantic.c src/interpreter.c src/parser.tab.c src/lex.yy.c
OBJS = src/main.o src/error.o src/ast.o src/symtab.o src/semantic.o src/interpreter.o src/parser.tab.o src/lex.yy.o

.PHONY: all clean test demo exemplos help

all: $(TARGET)

src/parser.tab.c src/parser.tab.h: src/parser.y
	bison -d -v -o src/parser.tab.c src/parser.y

$(OBJS): src/parser.tab.h

src/lex.yy.c: src/lexer.l src/parser.tab.h
	flex -o $@ src/lexer.l

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

src/parser.tab.o: src/parser.tab.c
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@

src/lex.yy.o: src/lex.yy.c src/parser.tab.h
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@

test: $(TARGET)
	./rodar_testes.sh

exemplos: $(TARGET)
	./exemplos/rodar_exemplos.sh

demo: $(TARGET)
	@echo "=================================================="
	@echo "1. Executando interpretador com script funcional:"
	@echo "=================================================="
	./$(TARGET) tests/valid/01_tokens_basicos.ghost
	@echo ""
	@echo "=================================================="
	@echo "2. Barreira ICE capturando divisao por zero:"
	@echo "=================================================="
	-./$(TARGET) tests/invalid/05_execucao_divisao_zero.ghost
	@echo "=================================================="
	@echo "Verificacoes do interpretador concluidas!"

clean:
	rm -f src/*.o $(TARGET)
	rm -f src/lex.yy.c src/parser.tab.c src/parser.tab.h src/parser.output

help:
	@echo "Alvos disponiveis no Makefile:"
	@echo "  make        - Compila o interpretador Ghost"
	@echo "  make test     - Roda a suite de testes (validos, erros lexicos, sintaticos e semanticos)"
	@echo "  make exemplos - Executa os programas de exemplo e mostra as saidas"
	@echo "  make demo     - Demonstracao rapida: um programa valido e um erro de execucao"
	@echo "  make clean  - Remove binarios e arquivos gerados"
	@echo "  make help   - Exibe esta mensagem de ajuda"
