#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "error.h"

#define GHOST_VERSION "0.2.0-dev (Etapa 2)"

static void print_banner(void) {
    printf("Ghost (in the shell) - Interpretador Cyberpunk\n");
    printf("Versao: %s\n", GHOST_VERSION);
}

static void print_help(const char *prog_name) {
    print_banner();
    printf("\nUso:\n");
    printf("  %s [opcoes] <arquivo.ghost>\n\n", prog_name);
    printf("Opcoes:\n");
    printf("  --tokens        Executa apenas a analise lexica e exibe os tokens\n");
    printf("  --ast           Gera a arvore sintatica abstrata (AST) e a exibe\n");
    printf("  --check         Executa apenas a analise lexica, sintatica e semantica\n");
    printf("  -v, --version   Exibe a versao do interpretador Ghost\n");
    printf("  -h, --help      Exibe esta mensagem de ajuda\n\n");
    printf("Se nenhum arquivo for especificado ou '-' for informado, le da entrada padrao (stdin).\n");
}

int main(int argc, char *argv[]) {
    const char *filename = NULL;
    int mode_tokens = 0;
    int mode_ast = 0;
    int mode_check = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_banner();
            return 0;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            mode_tokens = 1;
        } else if (strcmp(argv[i], "--ast") == 0) {
            mode_ast = 1;
        } else if (strcmp(argv[i], "--check") == 0) {
            mode_check = 1;
        } else if (argv[i][0] == '-' && strcmp(argv[i], "-") != 0) {
            fprintf(stderr, "Opcao desconhecida: %s\n", argv[i]);
            print_help(argv[0]);
            return 1;
        } else {
            filename = argv[i];
        }
    }

    /* Suprime warnings de variaveis nao usadas na Fase 0 */
    (void)mode_tokens;
    (void)mode_ast;
    (void)mode_check;

    if (filename) {
        printf("[GHOST] Carregando arquivo de instrucoes: %s\n", filename);
    } else {
        printf("[GHOST] Lendo do terminal padrao (stdin)...\n");
    }

    printf("[GHOST] Infraestrutura inicial operacional. Pronto para analise lexica.\n");
    return 0;
}
