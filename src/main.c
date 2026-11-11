#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "error.h"
#include "tokens.h"

#define GHOST_VERSION "0.2.0-dev (Etapa 2 - Fase 1: Lexer)"

extern FILE *yyin;
extern int yylex(void);
extern char *yytext;

static void print_banner(void) {
    printf("Ghost (in the shell) - Interpretador Cyberpunk\n");
    printf("Versao: %s\n", GHOST_VERSION);
}

static void print_help(const char *prog_name) {
    print_banner();
    printf("\nUso:\n");
    printf("  %s [opcoes] <arquivo.ghost>\n\n", prog_name);
    printf("Opcoes:\n");
    printf("  --tokens        Executa a analise lexica e exibe a lista de tokens\n");
    printf("  --ast           Gera a arvore sintatica abstrata (AST) e a exibe\n");
    printf("  --check         Executa analise lexica, sintatica e semantica\n");
    printf("  -v, --version   Exibe a versao do interpretador Ghost\n");
    printf("  -h, --help      Exibe esta mensagem de ajuda\n\n");
    printf("Se nenhum arquivo for especificado ou '-' for informado, le da entrada padrao (stdin).\n");
}

static int run_lexer(void) {
    int tok;
    printf("----------------------------------------------------------------------\n");
    printf("%-6s %-6s %-18s %s\n", "LINHA", "COLUNA", "TOKEN", "VALOR / LEXEMA");
    printf("----------------------------------------------------------------------\n");

    while ((tok = yylex()) != TOK_EOF) {
        printf("%-6d %-6d %-18s ", token_line, token_col, token_name(tok));
        switch (tok) {
            case TOK_NUM_CORE:
                printf("%ld\n", (long)ghost_lval.val_core);
                break;
            case TOK_NUM_FLUX:
                printf("%f\n", ghost_lval.val_flux);
                break;
            case TOK_SYNC:
                printf("sync (true)\n");
                break;
            case TOK_VOID:
                printf("void (false)\n");
                break;
            case TOK_STRING:
                printf("\"%s\"\n", ghost_lval.val_stream ? ghost_lval.val_stream : "");
                free(ghost_lval.val_stream);
                ghost_lval.val_stream = NULL;
                break;
            case TOK_ID:
                printf("%s\n", ghost_lval.val_id ? ghost_lval.val_id : "");
                free(ghost_lval.val_id);
                ghost_lval.val_id = NULL;
                break;
            default:
                printf("'%s'\n", yytext);
                break;
        }
    }
    printf("----------------------------------------------------------------------\n");

    if (ice_get_error_count() > 0) {
        fprintf(stderr, "\n[GHOST] Analise lexica encerrada com %d erro(s) ICE detectado(s).\n",
                ice_get_error_count());
        return 1;
    }

    printf("\n[GHOST] Analise lexica concluida com sucesso. 0 erros encontrados.\n");
    return 0;
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

    /* Suprime warnings de variaveis preparadas para as proximas fases */
    (void)mode_tokens;
    (void)mode_ast;
    (void)mode_check;

    /* Abre arquivo de entrada ou le de stdin */
    if (filename && strcmp(filename, "-") != 0) {
        yyin = fopen(filename, "r");
        if (!yyin) {
            perror("[GHOST::ICE_FAULT] Falha ao abrir arquivo fonte");
            return 1;
        }
    } else {
        yyin = stdin;
    }

    /* Na Fase 1, o modo default ou com --tokens roda a analise lexica */
    int ret = run_lexer();

    if (yyin != stdin) {
        fclose(yyin);
    }

    return ret;
}
