CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc
LDFLAGS = 

TARGET = ghost
SRCS = src/main.c src/error.c src/lex.yy.c
OBJS = src/main.o src/error.o src/lex.yy.o

.PHONY: all clean test help

all: $(TARGET)

src/lex.yy.c: src/lexer.l src/tokens.h
	flex -o $@ $<

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

src/lex.yy.o: src/lex.yy.c
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@

test: $(TARGET)
	@echo "=================================================="
	@echo "1. Testando analise lexica com script valido..."
	@echo "=================================================="
	./$(TARGET) tests/valid/01_tokens_basicos.ghost
	@echo ""
	@echo "=================================================="
	@echo "2. Testando barreira ICE com erros lexicos..."
	@echo "=================================================="
	-./$(TARGET) tests/invalid/01_lexico_invalido.ghost
	@echo "=================================================="
	@echo "Verificacao lexica concluida!"

clean:
	rm -f src/*.o $(TARGET)
	rm -f src/lex.yy.c src/parser.tab.c src/parser.tab.h src/parser.output

help:
	@echo "Alvos disponiveis no Makefile:"
	@echo "  make        - Compila o interpretador Ghost"
	@echo "  make test   - Executa a verificacao lexica"
	@echo "  make clean  - Remove binarios e arquivos gerados"
	@echo "  make help   - Exibe esta mensagem de ajuda"
