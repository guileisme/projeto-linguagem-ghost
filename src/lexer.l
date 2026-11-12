%{
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "types.h"
#include "ast.h"
#include "error.h"
#include "parser.tab.h"

int current_line = 1;
int current_col = 1;
int token_line = 1;
int token_col = 1;

/* Posicao onde o ultimo comentario de bloco foi aberto (para o erro de comentario nao fechado) */
static int comment_line = 1;
static int comment_col = 1;

static char *unescape_string(const char *src) {
    size_t len = strlen(src);
    if (len < 2) return strdup("");
    
    char *dest = (char *)malloc(len);
    if (!dest) return NULL;
    
    size_t di = 0;
    /* Pula aspas inicial e final */
    for (size_t si = 1; si < len - 1; si++) {
        if (src[si] == '\\' && si + 1 < len - 1) {
            si++;
            switch (src[si]) {
                case 'n':  dest[di++] = '\n'; break;
                case 't':  dest[di++] = '\t'; break;
                case 'r':  dest[di++] = '\r'; break;
                case '\\': dest[di++] = '\\'; break;
                case '\"': dest[di++] = '\"'; break;
                default:   dest[di++] = src[si]; break;
            }
        } else {
            dest[di++] = src[si];
        }
    }
    dest[di] = '\0';
    return dest;
}

#define YY_USER_ACTION \
    token_line = current_line; \
    token_col = current_col; \
    for (int i = 0; yytext[i] != '\0'; i++) { \
        if (yytext[i] == '\n') { \
            current_line++; \
            current_col = 1; \
        } else if (yytext[i] == '\t') { \
            current_col += 4 - ((current_col - 1) % 4); \
        } else { \
            current_col++; \
        } \
    }

%}

%option noyywrap
%option nounput
%option noinput

%x COMMENT_BLOCK

%%

    /* Shebang (ignorado se presente) */
^#!.*                       {
                                /* shebang so e valido na primeira linha */
                                if (token_line != 1) {
                                    ice_error(ICE_LEXICAL, token_line, token_col,
                                              "diretiva shebang '#!' so e permitida na primeira linha");
                                }
                            }

    /* Comentarios de linha unica */
"//".*                      { /* ignora comentario inline */ }

    /* Comentarios de bloco */
"/*"                        { comment_line = token_line; comment_col = token_col; BEGIN(COMMENT_BLOCK); }
<COMMENT_BLOCK>"*/"         { BEGIN(INITIAL); }
<COMMENT_BLOCK>\n           { /* quebra de linha dentro do comentario */ }
<COMMENT_BLOCK>.            { /* conteudo do comentario */ }
<COMMENT_BLOCK><<EOF>>      {
                                ice_error(ICE_LEXICAL, comment_line, comment_col, "bloco de comentario '/*' aberto e nunca fechado");
                                return 0;
                            }

    /* Espacos em branco */
[ \t\r\n]+                  { /* ignora espacos em branco */ }

    /* Palavras Reservadas da Linguagem Ghost */
"dive"                      { return DIVE; }
"surface"                   { return SURFACE; }
"implant"                   { return IMPLANT; }
"core"                      { return T_CORE; }
"flux"                      { return T_FLUX; }
"state"                     { return T_STATE; }
"stream"                    { return T_STREAM; }
"sync"                      { yylval.val_state = true; return SYNC; }
"void"                      { yylval.val_state = false; return VOID; }
"when"                      { return WHEN; }
"otherwise"                 { return OTHERWISE; }
"cycle"                     { return CYCLE; }
"sweep"                     { return SWEEP; }
"from"                      { return FROM; }
"to"                        { return TO; }
"route"                     { return ROUTE; }
"in"                        { return IN; }
"cloak"                     { return CLOAK; }
"bypass"                    { return BYPASS; }
"whisper"                   { return WHISPER; }
"scan"                      { return SCAN; }
"shell"                     { return SHELL; }
"signal"                    { return SIGNAL; }
"eject"                     { return EJECT; }
"protocol"                  { return PROTOCOL; }
"returns"                   { return RETURNS; }
"yield"                     { return YIELD; }
"exec"                      { return EXEC; }
"with"                      { return WITH; }
"either"                    { return EITHER; }
"never"                     { return NEVER; }

    /* Operadores compostos */
"<~"                        { return INJECT; }
"=/="                       { return NEQ; }
"<="                        { return LE; }
">="                        { return GE; }

    /* Operadores simples e pontuacoes */
"!"                         { return TRIGGER; }
"&"                         { return JOIN; }
"+"                         { return PLUS; }
"-"                         { return MINUS; }
"*"                         { return TIMES; }
"/"                         { return DIV; }
"%"                         { return MOD; }
"="                         { return EQ; }
"<"                         { return LT; }
">"                         { return GT; }
"("                         { return LPAREN; }
")"                         { return RPAREN; }
","                         { return COMMA; }
":"                         { return COLON; }

    /* Literais Numericos Reais (flux) */
[0-9]+\.[0-9]+              {
                                yylval.val_flux = strtod(yytext, NULL);
                                return NUM_FLUX;
                            }

    /* Literais Numericos Inteiros (core) */
[0-9]+                      {
                                yylval.val_core = strtoll(yytext, NULL, 10);
                                return NUM_CORE;
                            }

    /* Cadeias de Texto (stream) */
\"([^"\\]|\\.)*\"           {
                                yylval.val_stream = unescape_string(yytext);
                                return STRING;
                            }

    /* String nao terminada */
\"([^"\\]|\\.)*             {
                                ice_error(ICE_LEXICAL, token_line, token_col, "cadeia de texto (stream) nao terminada");
                            }

    /* Identificador iniciado por digito (ex.: 9vidas): erro lexico */
[0-9]+[a-zA-Z_][a-zA-Z0-9_]* {
                                ice_error(ICE_LEXICAL, token_line, token_col,
                                          "identificador '%s' nao pode comecar com digito", yytext);
                                yylval.val_id = strdup(yytext);
                                return ID; /* segue como identificador para nao gerar erros em cascata */
                            }

    /* Identificadores */
[a-zA-Z_][a-zA-Z0-9_]*      {
                                yylval.val_id = strdup(yytext);
                                return ID;
                            }

    /* Caracteres fora do ASCII (acentos etc.): uma sequencia vira um unico erro */
[\x80-\xFF]+                {
                                ice_error(ICE_LEXICAL, token_line, token_col, "caractere invalido '%s'", yytext);
                            }

    /* Tratamento de Erros Lexicos: qualquer caractere desconhecido */
.                           {
                                ice_error(ICE_LEXICAL, token_line, token_col, "caractere invalido '%s'", yytext);
                            }

%%

const char *token_to_name(int token) {
    switch (token) {
        case 0:         return "EOF";
        case DIVE:      return "DIVE";
        case SURFACE:   return "SURFACE";
        case IMPLANT:   return "IMPLANT";
        case T_CORE:    return "T_CORE";
        case T_FLUX:    return "T_FLUX";
        case T_STATE:   return "T_STATE";
        case T_STREAM:  return "T_STREAM";
        case SYNC:      return "SYNC";
        case VOID:      return "VOID";
        case WHEN:      return "WHEN";
        case OTHERWISE: return "OTHERWISE";
        case CYCLE:     return "CYCLE";
        case SWEEP:     return "SWEEP";
        case FROM:      return "FROM";
        case TO:        return "TO";
        case ROUTE:     return "ROUTE";
        case IN:        return "IN";
        case CLOAK:     return "CLOAK";
        case BYPASS:    return "BYPASS";
        case WHISPER:   return "WHISPER";
        case SCAN:      return "SCAN";
        case SHELL:     return "SHELL";
        case SIGNAL:    return "SIGNAL";
        case EJECT:     return "EJECT";
        case PROTOCOL:  return "PROTOCOL";
        case RETURNS:   return "RETURNS";
        case YIELD:     return "YIELD";
        case EXEC:      return "EXEC";
        case WITH:      return "WITH";
        case EITHER:    return "EITHER";
        case NEVER:     return "NEVER";
        case INJECT:    return "INJECT (<~)";
        case TRIGGER:   return "TRIGGER (!)";
        case JOIN:      return "JOIN (&)";
        case PLUS:      return "PLUS (+)";
        case MINUS:     return "MINUS (-)";
        case TIMES:     return "TIMES (*)";
        case DIV:       return "DIV (/)";
        case MOD:       return "MOD (%)";
        case EQ:        return "EQ (=)";
        case NEQ:       return "NEQ (=/=)";
        case LT:        return "LT (<)";
        case GT:        return "GT (>)";
        case LE:        return "LE (<=)";
        case GE:        return "GE (>=)";
        case LPAREN:    return "LPAREN";
        case RPAREN:    return "RPAREN";
        case COMMA:     return "COMMA";
        case COLON:     return "COLON";
        case ID:        return "ID";
        case NUM_CORE:  return "NUM_CORE";
        case NUM_FLUX:  return "NUM_FLUX";
        case STRING:    return "STRING";
        default:        return "DESCONHECIDO";
    }
}
