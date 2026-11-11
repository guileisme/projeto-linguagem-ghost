#ifndef GHOST_TYPES_H
#define GHOST_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Tipos de Dados da Linguagem Ghost (in the shell)
 * - core:   Inteiros de 64 bits (PIDs, ciclos de clock, portas)
 * - flux:   Numeros reais de ponto flutuante de dupla precisao
 * - state:  Valores logicos de integridade (sync = true, void = false)
 * - stream: Cadeias de texto e fluxos de barramento/terminal
 */
typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_CORE,
    TYPE_FLUX,
    TYPE_STATE,
    TYPE_STREAM,
    TYPE_NONE
} DataType;

static inline const char *type_to_string(DataType type) {
    switch (type) {
        case TYPE_CORE:   return "core";
        case TYPE_FLUX:   return "flux";
        case TYPE_STATE:  return "state";
        case TYPE_STREAM: return "stream";
        case TYPE_NONE:   return "none";
        default:          return "unknown";
    }
}

#endif /* GHOST_TYPES_H */
