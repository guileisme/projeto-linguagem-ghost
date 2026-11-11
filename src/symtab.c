#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Scope *scope_new(int level, Scope *parent) {
    Scope *s = (Scope *)calloc(1, sizeof(Scope));
    if (!s) {
        fprintf(stderr, "[GHOST::ICE_FAULT] Falha ao alocar memoria para escopo\n");
        exit(1);
    }
    s->level = level;
    s->parent = parent;
    s->symbols = NULL;
    return s;
}

static void scope_free(Scope *s) {
    if (!s) return;
    Symbol *sym = s->symbols;
    while (sym) {
        Symbol *next = sym->next;
        free(sym->name);
        free(sym);
        sym = next;
    }
    free(s);
}

SymbolTable *symtab_new(void) {
    SymbolTable *st = (SymbolTable *)calloc(1, sizeof(SymbolTable));
    if (!st) {
        fprintf(stderr, "[GHOST::ICE_FAULT] Falha ao alocar tabela de simbolos\n");
        exit(1);
    }
    st->global_scope = scope_new(0, NULL);
    st->current_scope = st->global_scope;

    /* Registra identificador interno signal (retorno do terminal) */
    symtab_insert(st, "signal", TYPE_CORE, 0, 0);
    Symbol *sig = symtab_lookup(st, "signal");
    if (sig) {
        sig->is_readonly = true;
        sig->is_initialized = true;
    }

    return st;
}

void symtab_free(SymbolTable *st) {
    if (!st) return;
    Scope *s = st->current_scope;
    while (s) {
        Scope *parent = s->parent;
        scope_free(s);
        s = parent;
    }
    free(st);
}

void symtab_enter_scope(SymbolTable *st) {
    if (!st) return;
    Scope *new_scope = scope_new(st->current_scope->level + 1, st->current_scope);
    st->current_scope = new_scope;
}

void symtab_exit_scope(SymbolTable *st) {
    if (!st || !st->current_scope) return;
    if (st->current_scope->parent) {
        Scope *old_scope = st->current_scope;
        st->current_scope = old_scope->parent;
        scope_free(old_scope);
    }
}

bool symtab_insert(SymbolTable *st, const char *name, DataType type, int line, int col) {
    if (!st || !st->current_scope || !name) return false;

    /* Verifica se ja existe no escopo atual */
    if (symtab_lookup_current(st, name)) {
        return false;
    }

    Symbol *sym = (Symbol *)calloc(1, sizeof(Symbol));
    if (!sym) {
        fprintf(stderr, "[GHOST::ICE_FAULT] Falha ao alocar simbolo\n");
        exit(1);
    }
    sym->name = strdup(name);
    sym->type = type;
    sym->line = line;
    sym->col = col;
    sym->is_initialized = false;
    sym->is_readonly = false;

    sym->next = st->current_scope->symbols;
    st->current_scope->symbols = sym;
    return true;
}

Symbol *symtab_lookup_current(SymbolTable *st, const char *name) {
    if (!st || !st->current_scope || !name) return NULL;
    for (Symbol *s = st->current_scope->symbols; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

Symbol *symtab_lookup(SymbolTable *st, const char *name) {
    if (!st || !name) return NULL;
    for (Scope *sc = st->current_scope; sc; sc = sc->parent) {
        for (Symbol *s = sc->symbols; s; s = s->next) {
            if (strcmp(s->name, name) == 0) {
                return s;
            }
        }
    }
    return NULL;
}
