%{
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "tokens.h"
#include "error.h"

GhostYYSTYPE ghost_lval;
int current_line = 1;
int current_col = 1;
int token_line = 1;
int token_col = 1;

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
^#!.*                       { /* ignora shebang */ }

    /* Comentarios de linha unica */
"//".*                      { /* ignora comentario inline */ }

    /* Comentarios de bloco */
"/*"                        { BEGIN(COMMENT_BLOCK); }
<COMMENT_BLOCK>"*/"         { BEGIN(INITIAL); }
<COMMENT_BLOCK>\n           { /* quebra de linha dentro do comentario */ }
<COMMENT_BLOCK>.            { /* conteudo do comentario */ }
<COMMENT_BLOCK><<EOF>>      {
                                ice_error(ICE_LEXICAL, token_line, token_col, "bloco de comentario nao finalizado antes do fim do arquivo");
                                return TOK_EOF;
                            }

    /* Espacos em branco */
[ \t\r\n]+                  { /* ignora espacos em branco */ }

    /* Palavras Reservadas da Linguagem Ghost */
"dive"                      { return TOK_DIVE; }
"surface"                   { return TOK_SURFACE; }
"implant"                   { return TOK_IMPLANT; }
"core"                      { return TOK_T_CORE; }
"flux"                      { return TOK_T_FLUX; }
"state"                     { return TOK_T_STATE; }
"stream"                    { return TOK_T_STREAM; }
"sync"                      { ghost_lval.val_state = true; return TOK_SYNC; }
"void"                      { ghost_lval.val_state = false; return TOK_VOID; }
"when"                      { return TOK_WHEN; }
"otherwise"                 { return TOK_OTHERWISE; }
"cycle"                     { return TOK_CYCLE; }
"sweep"                     { return TOK_SWEEP; }
"from"                      { return TOK_FROM; }
"to"                        { return TOK_TO; }
"route"                     { return TOK_ROUTE; }
"in"                        { return TOK_IN; }
"cloak"                     { return TOK_CLOAK; }
"bypass"                    { return TOK_BYPASS; }
"whisper"                   { return TOK_WHISPER; }
"scan"                      { return TOK_SCAN; }
"shell"                     { return TOK_SHELL; }
"signal"                    { return TOK_SIGNAL; }
"eject"                     { return TOK_EJECT; }
"protocol"                  { return TOK_PROTOCOL; }
"returns"                   { return TOK_RETURNS; }
"yield"                     { return TOK_YIELD; }
"exec"                      { return TOK_EXEC; }
"with"                      { return TOK_WITH; }
"either"                    { return TOK_EITHER; }
"never"                     { return TOK_NEVER; }

    /* Operadores compostos */
"<~"                        { return TOK_INJECT; }
"=/="                       { return TOK_NEQ; }
"<="                        { return TOK_LE; }
">="                        { return TOK_GE; }

    /* Operadores simples e pontuacoes */
"!"                         { return TOK_TRIGGER; }
"&"                         { return TOK_JOIN; }
"+"                         { return TOK_PLUS; }
"-"                         { return TOK_MINUS; }
"*"                         { return TOK_TIMES; }
"/"                         { return TOK_DIV; }
"%"                         { return TOK_MOD; }
"="                         { return TOK_EQ; }
"<"                         { return TOK_LT; }
">"                         { return TOK_GT; }
"("                         { return TOK_LPAREN; }
")"                         { return TOK_RPAREN; }
","                         { return TOK_COMMA; }
":"                         { return TOK_COLON; }

    /* Literais Numericos Reais (flux) */
[0-9]+\.[0-9]+              {
                                ghost_lval.val_flux = strtod(yytext, NULL);
                                ghost_lval.line = token_line;
                                ghost_lval.col = token_col;
                                return TOK_NUM_FLUX;
                            }

    /* Literais Numericos Inteiros (core) */
[0-9]+                      {
                                ghost_lval.val_core = strtoll(yytext, NULL, 10);
                                ghost_lval.line = token_line;
                                ghost_lval.col = token_col;
                                return TOK_NUM_CORE;
                            }

    /* Cadeias de Texto (stream) */
\"([^"\\]|\\.)*\"           {
                                ghost_lval.val_stream = unescape_string(yytext);
                                ghost_lval.line = token_line;
                                ghost_lval.col = token_col;
                                return TOK_STRING;
                            }

    /* String nao terminada */
\"([^"\\]|\\.)*             {
                                ice_error(ICE_LEXICAL, token_line, token_col, "cadeia de texto (stream) nao terminada");
                            }

    /* Identificadores */
[a-zA-Z_][a-zA-Z0-9_]*      {
                                ghost_lval.val_id = strdup(yytext);
                                ghost_lval.line = token_line;
                                ghost_lval.col = token_col;
                                return TOK_ID;
                            }

    /* Tratamento de Erros Lexicos: qualquer caractere desconhecido */
.                           {
                                ice_error(ICE_LEXICAL, token_line, token_col, "caractere invalido '%s'", yytext);
                            }

%%

const char *token_name(int token) {
    switch (token) {
        case TOK_EOF:       return "EOF";
        case TOK_DIVE:      return "DIVE";
        case TOK_SURFACE:   return "SURFACE";
        case TOK_IMPLANT:   return "IMPLANT";
        case TOK_T_CORE:    return "T_CORE";
        case TOK_T_FLUX:    return "T_FLUX";
        case TOK_T_STATE:   return "T_STATE";
        case TOK_T_STREAM:  return "T_STREAM";
        case TOK_SYNC:      return "SYNC";
        case TOK_VOID:      return "VOID";
        case TOK_WHEN:      return "WHEN";
        case TOK_OTHERWISE: return "OTHERWISE";
        case TOK_CYCLE:     return "CYCLE";
        case TOK_SWEEP:     return "SWEEP";
        case TOK_FROM:      return "FROM";
        case TOK_TO:        return "TO";
        case TOK_ROUTE:     return "ROUTE";
        case TOK_IN:        return "IN";
        case TOK_CLOAK:     return "CLOAK";
        case TOK_BYPASS:    return "BYPASS";
        case TOK_WHISPER:   return "WHISPER";
        case TOK_SCAN:      return "SCAN";
        case TOK_SHELL:     return "SHELL";
        case TOK_SIGNAL:    return "SIGNAL";
        case TOK_EJECT:     return "EJECT";
        case TOK_PROTOCOL:  return "PROTOCOL";
        case TOK_RETURNS:   return "RETURNS";
        case TOK_YIELD:     return "YIELD";
        case TOK_EXEC:      return "EXEC";
        case TOK_WITH:      return "WITH";
        case TOK_EITHER:    return "EITHER";
        case TOK_NEVER:     return "NEVER";
        case TOK_INJECT:    return "INJECT (<~)";
        case TOK_TRIGGER:   return "TRIGGER (!)";
        case TOK_JOIN:      return "JOIN (&)";
        case TOK_PLUS:      return "PLUS (+)";
        case TOK_MINUS:     return "MINUS (-)";
        case TOK_TIMES:     return "TIMES (*)";
        case TOK_DIV:       return "DIV (/)";
        case TOK_MOD:       return "MOD (%)";
        case TOK_EQ:        return "EQ (=)";
        case TOK_NEQ:       return "NEQ (=/=)";
        case TOK_LT:        return "LT (<)";
        case TOK_GT:        return "GT (>)";
        case TOK_LE:        return "LE (<=)";
        case TOK_GE:        return "GE (>=)";
        case TOK_LPAREN:    return "LPAREN";
        case TOK_RPAREN:    return "RPAREN";
        case TOK_COMMA:     return "COMMA";
        case TOK_COLON:     return "COLON";
        case TOK_ID:        return "ID";
        case TOK_NUM_CORE:  return "NUM_CORE";
        case TOK_NUM_FLUX:  return "NUM_FLUX";
        case TOK_STRING:    return "STRING";
        default:            return "UNKNOWN";
    }
}
