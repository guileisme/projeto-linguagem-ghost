#ifndef GHOST_TOKENS_H
#define GHOST_TOKENS_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Codigos de Tokens da Linguagem Ghost (in the shell)
 * Compativeis com os gerados pelo Bison.
 */
typedef enum {
    TOK_EOF = 0,

    /* Palavras reservadas */
    TOK_DIVE = 258,
    TOK_SURFACE,
    TOK_IMPLANT,
    TOK_T_CORE,
    TOK_T_FLUX,
    TOK_T_STATE,
    TOK_T_STREAM,
    TOK_SYNC,
    TOK_VOID,
    TOK_WHEN,
    TOK_OTHERWISE,
    TOK_CYCLE,
    TOK_SWEEP,
    TOK_FROM,
    TOK_TO,
    TOK_ROUTE,
    TOK_IN,
    TOK_CLOAK,
    TOK_BYPASS,
    TOK_WHISPER,
    TOK_SCAN,
    TOK_SHELL,
    TOK_SIGNAL,
    TOK_EJECT,
    TOK_PROTOCOL,
    TOK_RETURNS,
    TOK_YIELD,
    TOK_EXEC,
    TOK_WITH,
    TOK_EITHER,
    TOK_NEVER,

    /* Operadores e Delimitadores */
    TOK_INJECT,   /* <~ */
    TOK_TRIGGER,  /* ! */
    TOK_JOIN,     /* & */
    TOK_PLUS,     /* + */
    TOK_MINUS,    /* - */
    TOK_TIMES,    /* * */
    TOK_DIV,      /* / */
    TOK_MOD,      /* % */
    TOK_EQ,       /* = */
    TOK_NEQ,      /* =/= */
    TOK_LT,       /* < */
    TOK_GT,       /* > */
    TOK_LE,       /* <= */
    TOK_GE,       /* >= */
    TOK_LPAREN,   /* ( */
    TOK_RPAREN,   /* ) */
    TOK_COMMA,    /* , */
    TOK_COLON,    /* : */

    /* Identificadores e Literais */
    TOK_ID,
    TOK_NUM_CORE,
    TOK_NUM_FLUX,
    TOK_STRING
} TokenType;

/* Valor semantico para os tokens */
typedef struct {
    int64_t val_core;
    double val_flux;
    bool val_state;
    char *val_stream;
    char *val_id;
    int line;
    int col;
} GhostYYSTYPE;

extern GhostYYSTYPE ghost_lval;
extern int token_line;
extern int token_col;

const char *token_name(int token);

#endif /* GHOST_TOKENS_H */
