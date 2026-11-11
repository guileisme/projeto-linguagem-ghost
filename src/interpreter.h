#ifndef GHOST_INTERPRETER_H
#define GHOST_INTERPRETER_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"
#include "ast.h"

/*
 * Estrutura de Valor em Tempo de Execucao
 */
typedef struct {
    DataType type;
    union {
        int64_t val_core;
        double val_flux;
        bool val_state;
        char *val_stream;
    };
} Value;

/*
 * Sinal de Controle de Fluxo
 */
typedef enum {
    FLOW_NORMAL = 0,
    FLOW_CLOAK,    /* break */
    FLOW_BYPASS,   /* continue */
    FLOW_EJECT     /* emergency disconnect */
} FlowSignal;

/*
 * Executa o programa Ghost a partir da raiz da AST
 * Retorna o codigo de saida do programa (0 para sucesso, ou valor do eject/erro)
 */
int interpret(ASTNode *root);

#endif /* GHOST_INTERPRETER_H */
