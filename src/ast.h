#ifndef GHOST_AST_H
#define GHOST_AST_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

/*
 * Tipos de Nos da Arvore Sintatica Abstrata (AST)
 */
typedef enum {
    /* Estrutura global e blocos */
    AST_PROGRAM,
    AST_BLOCK,

    /* Comandos */
    AST_IMPLANT,
    AST_ASSIGN,
    AST_WHISPER,
    AST_SCAN,
    AST_WHEN,
    AST_CYCLE,
    AST_SWEEP,
    AST_ROUTE,
    AST_CLOAK,
    AST_BYPASS,
    AST_SHELL_CMD,
    AST_EJECT,
    AST_PROTOCOL,
    AST_YIELD,
    AST_EXEC,

    /* Expressoes e Literais */
    AST_LITERAL_CORE,
    AST_LITERAL_FLUX,
    AST_LITERAL_STATE,
    AST_LITERAL_STREAM,
    AST_ID,
    AST_SIGNAL,
    AST_BINOP,
    AST_UNOP,
    AST_CALL,
    AST_SHELL_EXPR
} ASTNodeType;

/* Operadores Binarios */
typedef enum {
    BINOP_ADD,    /* + */
    BINOP_SUB,    /* - */
    BINOP_MUL,    /* * */
    BINOP_DIV,    /* / */
    BINOP_MOD,    /* % */
    BINOP_JOIN,   /* & */
    BINOP_EQ,     /* = */
    BINOP_NEQ,    /* =/= */
    BINOP_LT,     /* < */
    BINOP_GT,     /* > */
    BINOP_LE,     /* <= */
    BINOP_GE,     /* >= */
    BINOP_AND,    /* with */
    BINOP_OR      /* either */
} BinaryOp;

/* Operadores Unarios */
typedef enum {
    UNOP_NOT,     /* never */
    UNOP_NEG      /* - */
} UnaryOp;

struct ASTNode;

/* Lista encadeada de nos da AST */
typedef struct ASTNodeList {
    struct ASTNode *node;
    struct ASTNodeList *next;
} ASTNodeList;

/* Lista de parametros para protocolos */
typedef struct ParamNode {
    char *name;
    DataType type;
    struct ParamNode *next;
} ParamNode;

/* Estrutura principal do No da AST */
typedef struct ASTNode {
    ASTNodeType type;
    DataType expr_type; /* Tipo inferido na checagem semantica */
    int line;
    int col;

    union {
        /* Literais */
        int64_t literal_core;
        double literal_flux;
        bool literal_state;
        char *literal_stream;

        /* Identificador */
        char *id_name;

        /* Operacao Binaria */
        struct {
            BinaryOp op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binop;

        /* Operacao Unaria */
        struct {
            UnaryOp op;
            struct ASTNode *operand;
        } unop;

        /* Declaracao de variavel: implant */
        struct {
            char *name;
            DataType var_type;
            struct ASTNode *init_expr; /* pode ser NULL */
        } implant;

        /* Atribuicao: <~ */
        struct {
            char *name;
            struct ASTNode *expr;
        } assign;

        /* Saida: whisper */
        struct {
            ASTNodeList *args;
        } whisper;

        /* Entrada: scan */
        struct {
            char *name;
        } scan;

        /* Condicional: when / otherwise */
        struct {
            struct ASTNode *cond;
            struct ASTNode *then_branch;
            struct ASTNode *else_branch; /* pode ser NULL */
        } when_stmt;

        /* Laco condicional: cycle */
        struct {
            struct ASTNode *cond;
            struct ASTNode *body;
        } cycle_stmt;

        /* Laco de contagem: sweep */
        struct {
            char *var_name;
            struct ASTNode *from_expr;
            struct ASTNode *to_expr;
            struct ASTNode *body;
        } sweep_stmt;

        /* Laco de linhas: route in */
        struct {
            char *var_name;
            struct ASTNode *expr;
            struct ASTNode *body;
        } route_stmt;

        /* Execucao de terminal: shell "cmd"! */
        struct {
            struct ASTNode *cmd_expr;
        } shell_cmd;

        /* Expressao shell: shell("cmd") */
        struct {
            struct ASTNode *cmd_expr;
        } shell_expr;

        /* Desconexao de emergencia: eject */
        struct {
            struct ASTNode *code_expr; /* pode ser NULL */
        } eject_stmt;

        /* Bloco de comandos: dive ... surface */
        struct {
            ASTNodeList *stmts;
        } block;

        /* Programa completo */
        struct {
            ASTNodeList *stmts;
        } program;

        /* Chamada de procedimento ou funcao */
        struct {
            char *name;
            ASTNodeList *args;
        } call;

        /* Declaracao de protocolo */
        struct {
            char *name;
            ParamNode *params;
            DataType return_type;
            struct ASTNode *body;
        } protocol;

        /* Retorno de protocolo: yield */
        struct {
            struct ASTNode *expr;
        } yield_stmt;
    };
} ASTNode;

/* Construtores de Nos de Expressoes */
ASTNode *ast_new_literal_core(int64_t val, int line, int col);
ASTNode *ast_new_literal_flux(double val, int line, int col);
ASTNode *ast_new_literal_state(bool val, int line, int col);
ASTNode *ast_new_literal_stream(char *val, int line, int col);
ASTNode *ast_new_id(char *name, int line, int col);
ASTNode *ast_new_signal(int line, int col);
ASTNode *ast_new_binop(BinaryOp op, ASTNode *left, ASTNode *right, int line, int col);
ASTNode *ast_new_unop(UnaryOp op, ASTNode *operand, int line, int col);
ASTNode *ast_new_shell_expr(ASTNode *cmd_expr, int line, int col);
ASTNode *ast_new_call(char *name, ASTNodeList *args, int line, int col);

/* Construtores de Nos de Comandos */
ASTNode *ast_new_implant(char *name, DataType var_type, ASTNode *init_expr, int line, int col);
ASTNode *ast_new_assign(char *name, ASTNode *expr, int line, int col);
ASTNode *ast_new_whisper(ASTNodeList *args, int line, int col);
ASTNode *ast_new_scan(char *name, int line, int col);
ASTNode *ast_new_when(ASTNode *cond, ASTNode *then_branch, ASTNode *else_branch, int line, int col);
ASTNode *ast_new_cycle(ASTNode *cond, ASTNode *body, int line, int col);
ASTNode *ast_new_sweep(char *var_name, ASTNode *from_expr, ASTNode *to_expr, ASTNode *body, int line, int col);
ASTNode *ast_new_route(char *var_name, ASTNode *expr, ASTNode *body, int line, int col);
ASTNode *ast_new_cloak(int line, int col);
ASTNode *ast_new_bypass(int line, int col);
ASTNode *ast_new_shell_cmd(ASTNode *cmd_expr, int line, int col);
ASTNode *ast_new_eject(ASTNode *code_expr, int line, int col);
ASTNode *ast_new_block(ASTNodeList *stmts, int line, int col);
ASTNode *ast_new_program(ASTNodeList *stmts, int line, int col);
ASTNode *ast_new_protocol(char *name, ParamNode *params, DataType return_type, ASTNode *body, int line, int col);
ASTNode *ast_new_yield(ASTNode *expr, int line, int col);
ASTNode *ast_new_exec(ASTNode *call_node, int line, int col);

/* Utilitarios para Listas */
ASTNodeList *ast_list_new(ASTNode *node);
ASTNodeList *ast_list_append(ASTNodeList *list, ASTNode *node);
ParamNode *param_list_new(char *name, DataType type);
ParamNode *param_list_append(ParamNode *list, char *name, DataType type);

/* Utilitarios de Impressao e Memoria */
void ast_print(ASTNode *node, int indent);
void ast_free(ASTNode *node);
const char *binop_to_string(BinaryOp op);
const char *unop_to_string(UnaryOp op);

#endif /* GHOST_AST_H */
