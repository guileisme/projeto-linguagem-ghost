#include "error.h"
#include <stdio.h>
#include <stdarg.h>

static int g_error_count = 0;

static const char *category_to_string(IceCategory cat) {
    switch (cat) {
        case ICE_LEXICAL:  return "lexico";
        case ICE_SYNTAX:   return "sintatico";
        case ICE_SEMANTIC: return "semantico";
        case ICE_RUNTIME:  return "execucao";
        default:           return "desconhecido";
    }
}

void ice_error(IceCategory cat, int line, int col, const char *fmt, ...) {
    g_error_count++;
    fprintf(stderr, "[GHOST::ICE_FAULT] erro %s na linha %d, coluna %d: ",
            category_to_string(cat), line, col);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

int ice_get_error_count(void) {
    return g_error_count;
}

void ice_reset_error_count(void) {
    g_error_count = 0;
}
