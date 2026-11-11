#ifndef GHOST_SYMTAB_H
#define GHOST_SYMTAB_H

#include <stdbool.h>
#include "types.h"

/*
 * Estrutura de um Simbolo na Tabela
 */
typedef struct Symbol {
    char *name;
    DataType type;
    int line;
    int col;
    bool is_initialized;
    bool is_readonly;
    struct Symbol *next;
} Symbol;

/*
 * Estrutura de Escopo Encadeado (dive ... surface)
 */
typedef struct Scope {
    int level;
    Symbol *symbols;
    struct Scope *parent;
} Scope;

/*
 * Gerenciador da Tabela de Simbolos
 */
typedef struct SymbolTable {
    Scope *current_scope;
    Scope *global_scope;
} SymbolTable;

/* Funcoes da Tabela de Simbolos */
SymbolTable *symtab_new(void);
void symtab_free(SymbolTable *st);

void symtab_enter_scope(SymbolTable *st);
void symtab_exit_scope(SymbolTable *st);

bool symtab_insert(SymbolTable *st, const char *name, DataType type, int line, int col);
Symbol *symtab_lookup(SymbolTable *st, const char *name);
Symbol *symtab_lookup_current(SymbolTable *st, const char *name);

#endif /* GHOST_SYMTAB_H */
