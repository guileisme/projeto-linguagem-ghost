%{
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "types.h"
#include "ast.h"
#include "error.h"

extern int yylex(void);
extern int token_line;
extern int token_col;
extern char *yytext;

void yyerror(const char *msg);

ASTNode *g_ast_root = NULL;
%}

%define parse.error verbose

%union {
    int64_t val_core;
    double val_flux;
    bool val_state;
    char *val_stream;
    char *val_id;
    DataType data_type;
    ASTNode *ast_node;
    ASTNodeList *ast_list;
    ParamNode *param_list;
}

/* Palavras Reservadas */
%token DIVE SURFACE IMPLANT
%token T_CORE T_FLUX T_STATE T_STREAM
%token <val_state> SYNC VOID
%token WHEN OTHERWISE CYCLE SWEEP FROM TO ROUTE IN CLOAK BYPASS WHISPER SCAN SHELL SIGNAL EJECT PROTOCOL RETURNS YIELD EXEC
%token WITH EITHER NEVER

/* Operadores e Pontuacoes */
%token INJECT   /* <~ */
%token TRIGGER  /* ! */
%token JOIN     /* & */
%token PLUS MINUS TIMES DIV MOD
%token EQ NEQ LT GT LE GE
%token LPAREN RPAREN COMMA COLON

/* Literais e Identificadores */
%token <val_id> ID
%token <val_core> NUM_CORE
%token <val_flux> NUM_FLUX
%token <val_stream> STRING

/* Tipos nao-terminais */
%type <ast_node> script item comando declaracao atribuicao condicional
%type <ast_node> laco_cycle laco_sweep laco_route saida entrada exec_shell chamada
%type <ast_node> bloco elif_chain expr
%type <ast_node> protocol_decl
%type <ast_list> item_list comando_list expr_list opt_expr_list
%type <param_list> params opt_params
%type <data_type> tipo opt_returns

/*
 * Precedencia e Associatividade dos Operadores (Secao 6.3)
 * Do mais baixo (nivel 8) ao mais alto (nivel 2)
 */
%left EITHER
%left WITH
%nonassoc EQ NEQ
%nonassoc LT GT LE GE
%left PLUS MINUS JOIN
%left TIMES DIV MOD
%right NEVER UMINUS

%start script

%%

script
    : /* vazio */
        {
            g_ast_root = ast_new_program(NULL, 1, 1);
            $$ = g_ast_root;
        }
    | item_list
        {
            g_ast_root = ast_new_program($1, 1, 1);
            $$ = g_ast_root;
        }
    ;

item_list
    : item
        {
            $$ = ast_list_new($1);
        }
    | item_list item
        {
            $$ = ast_list_append($1, $2);
        }
    ;

item
    : comando
    | protocol_decl
    ;

bloco
    : DIVE comando_list SURFACE
        {
            $$ = ast_new_block($2, token_line, token_col);
        }
    ;

comando_list
    : /* vazio */
        {
            $$ = NULL;
        }
    | comando_list comando
        {
            $$ = ast_list_append($1, $2);
        }
    ;

comando
    : declaracao
    | atribuicao
    | condicional
    | laco_cycle
    | laco_sweep
    | laco_route
    | saida
    | entrada
    | exec_shell
    | EXEC chamada TRIGGER
        {
            $$ = ast_new_exec($2, token_line, token_col);
        }
    | CLOAK TRIGGER
        {
            $$ = ast_new_cloak(token_line, token_col);
        }
    | BYPASS TRIGGER
        {
            $$ = ast_new_bypass(token_line, token_col);
        }
    | YIELD expr TRIGGER
        {
            $$ = ast_new_yield($2, token_line, token_col);
        }
    | YIELD TRIGGER
        {
            $$ = ast_new_yield(NULL, token_line, token_col);
        }
    | EJECT expr TRIGGER
        {
            $$ = ast_new_eject($2, token_line, token_col);
        }
    | EJECT TRIGGER
        {
            $$ = ast_new_eject(NULL, token_line, token_col);
        }
    ;

declaracao
    : IMPLANT ID COLON tipo TRIGGER
        {
            $$ = ast_new_implant($2, $4, NULL, token_line, token_col);
        }
    | IMPLANT ID COLON tipo INJECT expr TRIGGER
        {
            $$ = ast_new_implant($2, $4, $6, token_line, token_col);
        }
    ;

tipo
    : T_CORE   { $$ = TYPE_CORE; }
    | T_FLUX   { $$ = TYPE_FLUX; }
    | T_STATE  { $$ = TYPE_STATE; }
    | T_STREAM { $$ = TYPE_STREAM; }
    ;

atribuicao
    : ID INJECT expr TRIGGER
        {
            $$ = ast_new_assign($1, $3, token_line, token_col);
        }
    ;

condicional
    : WHEN expr bloco elif_chain
        {
            $$ = ast_new_when($2, $3, $4, token_line, token_col);
        }
    ;

elif_chain
    : /* vazio */
        {
            $$ = NULL;
        }
    | OTHERWISE bloco
        {
            $$ = $2;
        }
    | OTHERWISE WHEN expr bloco elif_chain
        {
            $$ = ast_new_when($3, $4, $5, token_line, token_col);
        }
    ;

laco_cycle
    : CYCLE expr bloco
        {
            $$ = ast_new_cycle($2, $3, token_line, token_col);
        }
    ;

laco_sweep
    : SWEEP ID FROM expr TO expr bloco
        {
            $$ = ast_new_sweep($2, $4, $6, $7, token_line, token_col);
        }
    ;

laco_route
    : ROUTE ID IN expr bloco
        {
            $$ = ast_new_route($2, $4, $5, token_line, token_col);
        }
    ;

saida
    : WHISPER expr_list TRIGGER
        {
            $$ = ast_new_whisper($2, token_line, token_col);
        }
    ;

expr_list
    : expr
        {
            $$ = ast_list_new($1);
        }
    | expr_list COMMA expr
        {
            $$ = ast_list_append($1, $3);
        }
    ;

entrada
    : SCAN ID TRIGGER
        {
            $$ = ast_new_scan($2, token_line, token_col);
        }
    ;

exec_shell
    : SHELL expr TRIGGER
        {
            $$ = ast_new_shell_cmd($2, token_line, token_col);
        }
    ;

chamada
    : ID LPAREN opt_expr_list RPAREN
        {
            $$ = ast_new_call($1, $3, token_line, token_col);
        }
    ;

opt_expr_list
    : /* vazio */
        {
            $$ = NULL;
        }
    | expr_list
        {
            $$ = $1;
        }
    ;

protocol_decl
    : PROTOCOL ID LPAREN opt_params RPAREN opt_returns bloco
        {
            $$ = ast_new_protocol($2, $4, $6, $7, token_line, token_col);
        }
    ;

opt_params
    : /* vazio */
        {
            $$ = NULL;
        }
    | params
        {
            $$ = $1;
        }
    ;

params
    : ID COLON tipo
        {
            $$ = param_list_new($1, $3);
        }
    | params COMMA ID COLON tipo
        {
            $$ = param_list_append($1, $3, $5);
        }
    ;

opt_returns
    : /* vazio */
        {
            $$ = TYPE_NONE;
        }
    | RETURNS tipo
        {
            $$ = $2;
        }
    ;

expr
    : expr EITHER expr
        {
            $$ = ast_new_binop(BINOP_OR, $1, $3, token_line, token_col);
        }
    | expr WITH expr
        {
            $$ = ast_new_binop(BINOP_AND, $1, $3, token_line, token_col);
        }
    | NEVER expr
        {
            $$ = ast_new_unop(UNOP_NOT, $2, token_line, token_col);
        }
    | expr EQ expr
        {
            $$ = ast_new_binop(BINOP_EQ, $1, $3, token_line, token_col);
        }
    | expr NEQ expr
        {
            $$ = ast_new_binop(BINOP_NEQ, $1, $3, token_line, token_col);
        }
    | expr LT expr
        {
            $$ = ast_new_binop(BINOP_LT, $1, $3, token_line, token_col);
        }
    | expr GT expr
        {
            $$ = ast_new_binop(BINOP_GT, $1, $3, token_line, token_col);
        }
    | expr LE expr
        {
            $$ = ast_new_binop(BINOP_LE, $1, $3, token_line, token_col);
        }
    | expr GE expr
        {
            $$ = ast_new_binop(BINOP_GE, $1, $3, token_line, token_col);
        }
    | expr PLUS expr
        {
            $$ = ast_new_binop(BINOP_ADD, $1, $3, token_line, token_col);
        }
    | expr MINUS expr
        {
            $$ = ast_new_binop(BINOP_SUB, $1, $3, token_line, token_col);
        }
    | expr JOIN expr
        {
            $$ = ast_new_binop(BINOP_JOIN, $1, $3, token_line, token_col);
        }
    | expr TIMES expr
        {
            $$ = ast_new_binop(BINOP_MUL, $1, $3, token_line, token_col);
        }
    | expr DIV expr
        {
            $$ = ast_new_binop(BINOP_DIV, $1, $3, token_line, token_col);
        }
    | expr MOD expr
        {
            $$ = ast_new_binop(BINOP_MOD, $1, $3, token_line, token_col);
        }
    | MINUS expr %prec UMINUS
        {
            $$ = ast_new_unop(UNOP_NEG, $2, token_line, token_col);
        }
    | LPAREN expr RPAREN
        {
            $$ = $2;
        }
    | chamada
        {
            $$ = $1;
        }
    | SHELL LPAREN expr RPAREN
        {
            $$ = ast_new_shell_expr($3, token_line, token_col);
        }
    | ID
        {
            $$ = ast_new_id($1, token_line, token_col);
        }
    | SIGNAL
        {
            $$ = ast_new_signal(token_line, token_col);
        }
    | NUM_CORE
        {
            $$ = ast_new_literal_core($1, token_line, token_col);
        }
    | NUM_FLUX
        {
            $$ = ast_new_literal_flux($1, token_line, token_col);
        }
    | STRING
        {
            $$ = ast_new_literal_stream($1, token_line, token_col);
        }
    | SYNC
        {
            $$ = ast_new_literal_state(true, token_line, token_col);
        }
    | VOID
        {
            $$ = ast_new_literal_state(false, token_line, token_col);
        }
    ;

%%

void yyerror(const char *msg) {
    ice_error(ICE_SYNTAX, token_line, token_col, "%s proximo a '%s'", msg, yytext);
}
