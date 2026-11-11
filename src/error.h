#ifndef GHOST_ERROR_H
#define GHOST_ERROR_H

/*
 * Modulo de Diagnostico de Excecoes e Erros - Barreira ICE
 * Padrao especificado:
 * [GHOST::ICE_FAULT] erro <categoria> na linha L, coluna C: <descricao>
 */
typedef enum {
    ICE_LEXICAL,
    ICE_SYNTAX,
    ICE_SEMANTIC,
    ICE_RUNTIME
} IceCategory;

void ice_error(IceCategory cat, int line, int col, const char *fmt, ...);
int ice_get_error_count(void);
void ice_reset_error_count(void);

#endif /* GHOST_ERROR_H */
