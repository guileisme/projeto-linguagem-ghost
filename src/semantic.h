#ifndef GHOST_SEMANTIC_H
#define GHOST_SEMANTIC_H

#include <stdbool.h>
#include "ast.h"
#include "symtab.h"

/*
 * Analisador Semantico da Linguagem Ghost
 * - Checagem de declaracao previa de identificadores
 * - Checagem de redeclaracao no mesmo escopo
 * - Deteccao de escopos (dive ... surface)
 * - Compatibilidade e inferencia de tipos
 * - Promocao permitida (core -> flux)
 */
bool semantic_analyze(ASTNode *root);

#endif /* GHOST_SEMANTIC_H */
