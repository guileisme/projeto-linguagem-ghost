CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc
LDFLAGS = 

TARGET = ghost
SRCS = src/main.c src/error.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean test help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	@echo "=================================================="
	@echo "Executando verificacao basica da infraestrutura..."
	@echo "=================================================="
	./$(TARGET) --version
	./$(TARGET) --help > /dev/null
	@echo "Infraestrutura validada com sucesso!"

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f src/lex.yy.c src/parser.tab.c src/parser.tab.h src/parser.output

help:
	@echo "Alvos disponiveis no Makefile:"
	@echo "  make        - Compila o interpretador Ghost"
	@echo "  make test   - Executa teste basico do binario"
	@echo "  make clean  - Remove binarios e arquivos intermediarios"
	@echo "  make help   - Exibe esta mensagem de ajuda"
