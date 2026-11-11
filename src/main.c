#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "error.h"
#include "ast.h"
#include "parser.tab.h"
#include "semantic.h"

#define GHOST_VERSION "0.2.0-dev (Etapa 2 - Fase 4: Semantica)"

extern FILE *yyin;
extern int yylex(void);
extern int yyparse(void);
extern char *yytext;
extern int token_line;
extern int token_col;
extern ASTNode *g_ast_root;
extern const char *token_to_name(int token);

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
    printf("  --check         Executa analise lexica e sintatica sem executar\n");
    printf("  -v, --version   Exibe a versao do interpretador Ghost\n");
    printf("  -h, --help      Exibe esta mensagem de ajuda\n\n");
    printf("Se nenhum arquivo for especificado ou '-' for informado, le da entrada padrao (stdin).\n");
}

static int run_lexer(void) {
    int tok;
    printf("----------------------------------------------------------------------\n");
    printf("%-6s %-6s %-18s %s\n", "LINHA", "COLUNA", "TOKEN", "VALOR / LEXEMA");
    printf("----------------------------------------------------------------------\n");

    while ((tok = yylex()) != 0) {
        printf("%-6d %-6d %-18s ", token_line, token_col, token_to_name(tok));
        switch (tok) {
            case NUM_CORE:
                printf("%ld\n", (long)yylval.val_core);
                break;
            case NUM_FLUX:
                printf("%f\n", yylval.val_flux);
                break;
            case SYNC:
                printf("sync (true)\n");
                break;
            case VOID:
                printf("void (false)\n");
                break;
            case STRING:
                printf("\"%s\"\n", yylval.val_stream ? yylval.val_stream : "");
                free(yylval.val_stream);
                yylval.val_stream = NULL;
                break;
            case ID:
                printf("%s\n", yylval.val_id ? yylval.val_id : "");
                free(yylval.val_id);
                yylval.val_id = NULL;
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

    int ret = 0;

    if (mode_tokens) {
        ret = run_lexer();
    } else {
        /* Executa a analise sintatica e construcao da AST */
        int parse_res = yyparse();

        if (parse_res != 0 || ice_get_error_count() > 0) {
            fprintf(stderr, "\n[GHOST] Analise sintatica interrompida com %d erro(s) ICE.\n",
                    ice_get_error_count());
            ret = 1;
        } else {
            if (mode_ast) {
                printf("[GHOST] Arvore Sintatica Abstrata (AST):\n");
                printf("======================================================================\n");
                ast_print(g_ast_root, 0);
                printf("======================================================================\n");
            }

            /* Executa a analise semantica com tabela de simbolos e checagem de tipos */
            bool sem_ok = semantic_analyze(g_ast_root);

            if (!sem_ok || ice_get_error_count() > 0) {
                fprintf(stderr, "\n[GHOST] Analise semantica interrompida com %d erro(s) ICE.\n",
                        ice_get_error_count());
                ret = 1;
            } else {
                printf("[GHOST] Analise semantica concluida com sucesso. 0 erros encontrados.\n");
                ret = 0;
            }
        }

        if (g_ast_root) {
            ast_free(g_ast_root);
            g_ast_root = NULL;
        }
    }

    if (yyin != stdin) {
        fclose(yyin);
    }

    return ret;
}
