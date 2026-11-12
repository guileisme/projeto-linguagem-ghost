#include "semantic.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_stmt(ASTNode *node, SymbolTable *st);
static DataType check_expr(ASTNode *node, SymbolTable *st);

static bool is_numeric(DataType t) {
    return (t == TYPE_CORE || t == TYPE_FLUX);
}

/*
 * Estado da analise de protocolos e de laços
 */
typedef struct ProtoSig {
    ASTNode *node;
    struct ProtoSig *next;
} ProtoSig;

static ProtoSig *g_protos = NULL;       /* protocolos declarados */
static ASTNode *g_current_proto = NULL; /* protocolo em analise (NULL = fora) */
static int g_loop_depth = 0;            /* quantos cycle/sweep/route envolvem o ponto atual */

static ASTNode *find_proto(const char *name) {
    for (ProtoSig *p = g_protos; p; p = p->next) {
        if (strcmp(p->node->protocol.name, name) == 0) return p->node;
    }
    return NULL;
}

static void register_proto(ASTNode *node) {
    ProtoSig *p = (ProtoSig *)calloc(1, sizeof(ProtoSig));
    p->node = node;
    p->next = g_protos;
    g_protos = p;
}

static void clear_protos(void) {
    ProtoSig *p = g_protos;
    while (p) {
        ProtoSig *next = p->next;
        free(p);
        p = next;
    }
    g_protos = NULL;
}

/* Um valor do tipo 'got' pode ser usado onde se espera 'want'? (core promove a flux) */
static bool type_accepts(DataType want, DataType got) {
    return want == got || (want == TYPE_FLUX && got == TYPE_CORE);
}

/*
 * Chamada de funcao embutida (exists, env, arg) ou de protocolo do usuario.
 * as_expr = true quando o valor da chamada e usado (dentro de uma expressao);
 * false quando e um comando 'exec' (o valor e descartado).
 */
static DataType check_call(ASTNode *node, SymbolTable *st, bool as_expr) {
    const char *name = node->call.name;

    if (strcmp(name, "exists") == 0) {
        if (node->call.args) check_expr(node->call.args->node, st);
        node->expr_type = TYPE_STATE;
        return TYPE_STATE;
    } else if (strcmp(name, "env") == 0) {
        if (node->call.args) check_expr(node->call.args->node, st);
        node->expr_type = TYPE_STREAM;
        return TYPE_STREAM;
    } else if (strcmp(name, "arg") == 0) {
        if (node->call.args) {
            DataType t = check_expr(node->call.args->node, st);
            if (t != TYPE_CORE && t != TYPE_UNKNOWN) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "'arg' requer indice do tipo 'core', mas recebeu '%s'", type_to_string(t));
            }
        }
        node->expr_type = TYPE_STREAM;
        return TYPE_STREAM;
    }

    ASTNode *proto = find_proto(name);
    if (!proto) {
        ice_error(ICE_SEMANTIC, node->line, node->col, "protocolo '%s' nao declarado", name);
        for (ASTNodeList *a = node->call.args; a; a = a->next) check_expr(a->node, st);
        node->expr_type = TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }

    /* Aridade e tipos dos argumentos */
    int expected = 0, given = 0;
    for (ParamNode *p = proto->protocol.params; p; p = p->next) expected++;
    for (ASTNodeList *a = node->call.args; a; a = a->next) given++;
    if (expected != given) {
        ice_error(ICE_SEMANTIC, node->line, node->col,
                  "protocolo '%s' espera %d argumento(s), mas recebeu %d", name, expected, given);
    }
    ParamNode *p = proto->protocol.params;
    int pos = 1;
    for (ASTNodeList *a = node->call.args; a; a = a->next, pos++) {
        DataType t = check_expr(a->node, st);
        if (p) {
            if (t != TYPE_UNKNOWN && !type_accepts(p->type, t)) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "argumento %d do protocolo '%s' deve ser do tipo '%s', mas recebeu '%s'",
                          pos, name, type_to_string(p->type), type_to_string(t));
            }
            p = p->next;
        }
    }

    DataType rt = proto->protocol.return_type;
    if (as_expr && rt == TYPE_NONE) {
        ice_error(ICE_SEMANTIC, node->line, node->col,
                  "protocolo '%s' nao tem 'returns' e nao pode ser usado em expressao", name);
        node->expr_type = TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }
    node->expr_type = rt;
    return rt;
}

static DataType check_expr(ASTNode *node, SymbolTable *st) {
    if (!node) return TYPE_UNKNOWN;

    switch (node->type) {
        case AST_LITERAL_CORE:
            node->expr_type = TYPE_CORE;
            return TYPE_CORE;

        case AST_LITERAL_FLUX:
            node->expr_type = TYPE_FLUX;
            return TYPE_FLUX;

        case AST_LITERAL_STATE:
            node->expr_type = TYPE_STATE;
            return TYPE_STATE;

        case AST_LITERAL_STREAM:
            node->expr_type = TYPE_STREAM;
            return TYPE_STREAM;

        case AST_ID: {
            Symbol *sym = symtab_lookup(st, node->id_name);
            if (!sym) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "identificador '%s' nao alocado neste escopo", node->id_name);
                node->expr_type = TYPE_UNKNOWN;
                return TYPE_UNKNOWN;
            }
            node->expr_type = sym->type;
            return sym->type;
        }

        case AST_SIGNAL:
            node->expr_type = TYPE_CORE;
            return TYPE_CORE;

        case AST_SHELL_EXPR: {
            DataType t = check_expr(node->shell_expr.cmd_expr, st);
            if (t != TYPE_STREAM && t != TYPE_UNKNOWN) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "'shell' requer expressao do tipo 'stream', mas recebeu '%s'",
                          type_to_string(t));
            }
            node->expr_type = TYPE_STREAM;
            return TYPE_STREAM;
        }

        case AST_CALL:
            return check_call(node, st, true);

        case AST_UNOP: {
            DataType t = check_expr(node->unop.operand, st);
            if (t == TYPE_UNKNOWN) return TYPE_UNKNOWN;

            if (node->unop.op == UNOP_NOT) {
                if (t != TYPE_STATE) {
                    ice_error(ICE_SEMANTIC, node->line, node->col,
                              "operador logico 'never' requer tipo 'state', recebido '%s'",
                              type_to_string(t));
                    node->expr_type = TYPE_STATE;
                    return TYPE_STATE;
                }
                node->expr_type = TYPE_STATE;
                return TYPE_STATE;
            } else if (node->unop.op == UNOP_NEG) {
                if (!is_numeric(t)) {
                    ice_error(ICE_SEMANTIC, node->line, node->col,
                              "menos unario requer operando numerico (core ou flux), recebido '%s'",
                              type_to_string(t));
                    node->expr_type = TYPE_UNKNOWN;
                    return TYPE_UNKNOWN;
                }
                node->expr_type = t;
                return t;
            }
            return TYPE_UNKNOWN;
        }

        case AST_BINOP: {
            DataType tl = check_expr(node->binop.left, st);
            DataType tr = check_expr(node->binop.right, st);

            if (tl == TYPE_UNKNOWN || tr == TYPE_UNKNOWN) {
                node->expr_type = TYPE_UNKNOWN;
                return TYPE_UNKNOWN;
            }

            switch (node->binop.op) {
                case BINOP_ADD:
                case BINOP_SUB:
                case BINOP_MUL:
                case BINOP_DIV: {
                    if (!is_numeric(tl) || !is_numeric(tr)) {
                        ice_error(ICE_SEMANTIC, node->line, node->col,
                                  "operacao aritmetica '%s' nao suportada entre '%s' e '%s'",
                                  binop_to_string(node->binop.op),
                                  type_to_string(tl), type_to_string(tr));
                        node->expr_type = TYPE_UNKNOWN;
                        return TYPE_UNKNOWN;
                    }
                    /* Promocao implicita de core para flux */
                    DataType res = (tl == TYPE_FLUX || tr == TYPE_FLUX) ? TYPE_FLUX : TYPE_CORE;
                    node->expr_type = res;
                    return res;
                }

                case BINOP_MOD: {
                    if (tl != TYPE_CORE || tr != TYPE_CORE) {
                        ice_error(ICE_SEMANTIC, node->line, node->col,
                                  "operador de resto '%%' requer operandos inteiros (core), recebido '%s' e '%s'",
                                  type_to_string(tl), type_to_string(tr));
                        node->expr_type = TYPE_CORE;
                        return TYPE_CORE;
                    }
                    node->expr_type = TYPE_CORE;
                    return TYPE_CORE;
                }

                case BINOP_JOIN: {
                    if (tl != TYPE_STREAM || tr != TYPE_STREAM) {
                        ice_error(ICE_SEMANTIC, node->line, node->col,
                                  "operador de barramento '&' requer fluxos do tipo 'stream', recebido '%s' e '%s'",
                                  type_to_string(tl), type_to_string(tr));
                        node->expr_type = TYPE_STREAM;
                        return TYPE_STREAM;
                    }
                    node->expr_type = TYPE_STREAM;
                    return TYPE_STREAM;
                }

                case BINOP_LT:
                case BINOP_GT:
                case BINOP_LE:
                case BINOP_GE: {
                    if (!is_numeric(tl) || !is_numeric(tr)) {
                        ice_error(ICE_SEMANTIC, node->line, node->col,
                                  "operador relacional '%s' requer operandos numericos (core ou flux), recebido '%s' e '%s'",
                                  binop_to_string(node->binop.op),
                                  type_to_string(tl), type_to_string(tr));
                    }
                    node->expr_type = TYPE_STATE;
                    return TYPE_STATE;
                }

                case BINOP_EQ:
                case BINOP_NEQ: {
                    bool compatible = (tl == tr) || (is_numeric(tl) && is_numeric(tr));
                    if (!compatible) {
                        ice_error(ICE_SEMANTIC, node->line, node->col,
                                  "comparacao de igualdade '%s' entre tipos incompativeis: '%s' e '%s'",
                                  binop_to_string(node->binop.op),
                                  type_to_string(tl), type_to_string(tr));
                    }
                    node->expr_type = TYPE_STATE;
                    return TYPE_STATE;
                }

                case BINOP_AND:
                case BINOP_OR: {
                    if (tl != TYPE_STATE || tr != TYPE_STATE) {
                        ice_error(ICE_SEMANTIC, node->line, node->col,
                                  "operador logico '%s' requer operandos do tipo 'state', recebido '%s' e '%s'",
                                  binop_to_string(node->binop.op),
                                  type_to_string(tl), type_to_string(tr));
                    }
                    node->expr_type = TYPE_STATE;
                    return TYPE_STATE;
                }
            }
            break;
        }

        default:
            break;
    }

    return TYPE_UNKNOWN;
}

static void check_stmt(ASTNode *node, SymbolTable *st) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            /* Registra todos os protocolos antes de checar o resto, para que
             * possam ser chamados antes do ponto em que sao declarados. */
            for (ASTNodeList *l = node->program.stmts; l; l = l->next) {
                if (l->node->type == AST_PROTOCOL) {
                    if (find_proto(l->node->protocol.name)) {
                        ice_error(ICE_SEMANTIC, l->node->line, l->node->col,
                                  "protocolo '%s' ja declarado", l->node->protocol.name);
                    } else {
                        register_proto(l->node);
                    }
                }
            }
            for (ASTNodeList *l = node->program.stmts; l; l = l->next) {
                check_stmt(l->node, st);
            }
            break;

        case AST_BLOCK:
            symtab_enter_scope(st);
            for (ASTNodeList *l = node->block.stmts; l; l = l->next) {
                check_stmt(l->node, st);
            }
            symtab_exit_scope(st);
            break;

        case AST_IMPLANT: {
            if (symtab_lookup_current(st, node->implant.name)) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "identificador '%s' ja alocado neste escopo", node->implant.name);
            } else {
                symtab_insert(st, node->implant.name, node->implant.var_type, node->line, node->col);
            }

            if (node->implant.init_expr) {
                DataType init_t = check_expr(node->implant.init_expr, st);
                if (init_t != TYPE_UNKNOWN) {
                    bool compatible = (node->implant.var_type == init_t) ||
                                      (node->implant.var_type == TYPE_FLUX && init_t == TYPE_CORE);
                    if (!compatible) {
                        ice_error(ICE_SEMANTIC, node->line, node->col,
                                  "tipo incompativel na inicializacao de '%s': esperado '%s', recebido '%s'",
                                  node->implant.name,
                                  type_to_string(node->implant.var_type),
                                  type_to_string(init_t));
                    }
                }
            }
            break;
        }

        case AST_ASSIGN: {
            Symbol *sym = symtab_lookup(st, node->assign.name);
            if (!sym) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "identificador '%s' nao alocado neste escopo", node->assign.name);
                check_expr(node->assign.expr, st);
                return;
            }

            if (sym->is_readonly) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "identificador '%s' e somente leitura e nao pode ser reatribuido",
                          node->assign.name);
            }

            DataType val_t = check_expr(node->assign.expr, st);
            if (val_t != TYPE_UNKNOWN) {
                bool compatible = (sym->type == val_t) ||
                                  (sym->type == TYPE_FLUX && val_t == TYPE_CORE);
                if (!compatible) {
                    ice_error(ICE_SEMANTIC, node->line, node->col,
                              "atribuicao com tipo incompativel para '%s': variavel do tipo '%s', valor do tipo '%s'",
                              node->assign.name,
                              type_to_string(sym->type),
                              type_to_string(val_t));
                }
            }
            break;
        }

        case AST_WHISPER:
            for (ASTNodeList *l = node->whisper.args; l; l = l->next) {
                check_expr(l->node, st);
            }
            break;

        case AST_SCAN: {
            Symbol *sym = symtab_lookup(st, node->scan.name);
            if (!sym) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "identificador '%s' nao alocado neste escopo", node->scan.name);
            }
            break;
        }

        case AST_WHEN: {
            DataType cond_t = check_expr(node->when_stmt.cond, st);
            if (cond_t != TYPE_STATE && cond_t != TYPE_UNKNOWN) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "condicao de 'when' requer tipo 'state', recebido '%s'",
                          type_to_string(cond_t));
            }
            check_stmt(node->when_stmt.then_branch, st);
            if (node->when_stmt.else_branch) {
                check_stmt(node->when_stmt.else_branch, st);
            }
            break;
        }

        case AST_CYCLE: {
            DataType cond_t = check_expr(node->cycle_stmt.cond, st);
            if (cond_t != TYPE_STATE && cond_t != TYPE_UNKNOWN) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "condicao de 'cycle' requer tipo 'state', recebido '%s'",
                          type_to_string(cond_t));
            }
            g_loop_depth++;
            check_stmt(node->cycle_stmt.body, st);
            g_loop_depth--;
            break;
        }

        case AST_SWEEP: {
            symtab_enter_scope(st);
            symtab_insert(st, node->sweep_stmt.var_name, TYPE_CORE, node->line, node->col);
            DataType tf = check_expr(node->sweep_stmt.from_expr, st);
            DataType tt = check_expr(node->sweep_stmt.to_expr, st);
            if (tf != TYPE_CORE || tt != TYPE_CORE) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "faixa de contagem do 'sweep' requer limites do tipo 'core'");
            }
            g_loop_depth++;
            check_stmt(node->sweep_stmt.body, st);
            g_loop_depth--;
            symtab_exit_scope(st);
            break;
        }

        case AST_ROUTE: {
            symtab_enter_scope(st);
            symtab_insert(st, node->route_stmt.var_name, TYPE_STREAM, node->line, node->col);
            DataType te = check_expr(node->route_stmt.expr, st);
            if (te != TYPE_STREAM && te != TYPE_UNKNOWN) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "'route' requer fluxo do tipo 'stream'");
            }
            g_loop_depth++;
            check_stmt(node->route_stmt.body, st);
            g_loop_depth--;
            symtab_exit_scope(st);
            break;
        }

        case AST_SHELL_CMD: {
            DataType t = check_expr(node->shell_cmd.cmd_expr, st);
            if (t != TYPE_STREAM && t != TYPE_UNKNOWN) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "'shell' requer expressao do tipo 'stream', mas recebeu '%s'",
                          type_to_string(t));
            }
            break;
        }

        case AST_EJECT: {
            if (node->eject_stmt.code_expr) {
                DataType t = check_expr(node->eject_stmt.code_expr, st);
                if (t != TYPE_CORE && t != TYPE_UNKNOWN) {
                    ice_error(ICE_SEMANTIC, node->line, node->col,
                              "'eject' requer codigo de saida do tipo 'core', mas recebeu '%s'",
                              type_to_string(t));
                }
            }
            break;
        }

        case AST_PROTOCOL: {
            /* Declaracao ja registrada no inicio do programa; aqui checa o corpo */
            ASTNode *prev_proto = g_current_proto;
            int prev_depth = g_loop_depth;
            g_current_proto = node;
            g_loop_depth = 0; /* cloak/bypass nao atravessam a fronteira do protocolo */

            symtab_enter_scope(st);
            for (ParamNode *p = node->protocol.params; p; p = p->next) {
                symtab_insert(st, p->name, p->type, node->line, node->col);
            }
            check_stmt(node->protocol.body, st);
            symtab_exit_scope(st);

            g_current_proto = prev_proto;
            g_loop_depth = prev_depth;
            break;
        }

        case AST_EXEC:
            check_call(node->unop.operand, st, false);
            break;

        case AST_YIELD: {
            DataType t = node->yield_stmt.expr ? check_expr(node->yield_stmt.expr, st) : TYPE_NONE;
            if (!g_current_proto) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "'yield' so pode ser usado dentro de um protocolo");
                break;
            }
            const char *pname = g_current_proto->protocol.name;
            DataType rt = g_current_proto->protocol.return_type;
            if (node->yield_stmt.expr) {
                if (rt == TYPE_NONE) {
                    ice_error(ICE_SEMANTIC, node->line, node->col,
                              "protocolo '%s' nao declara 'returns', mas 'yield' devolve valor", pname);
                } else if (t != TYPE_UNKNOWN && !type_accepts(rt, t)) {
                    ice_error(ICE_SEMANTIC, node->line, node->col,
                              "protocolo '%s' deve retornar '%s', mas 'yield' devolve '%s'",
                              pname, type_to_string(rt), type_to_string(t));
                }
            } else if (rt != TYPE_NONE) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "protocolo '%s' deve retornar '%s', mas 'yield' nao devolve valor",
                          pname, type_to_string(rt));
            }
            break;
        }

        case AST_CLOAK:
            if (g_loop_depth == 0) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "'cloak' so pode ser usado dentro de cycle, sweep ou route");
            }
            break;

        case AST_BYPASS:
            if (g_loop_depth == 0) {
                ice_error(ICE_SEMANTIC, node->line, node->col,
                          "'bypass' so pode ser usado dentro de cycle, sweep ou route");
            }
            break;

        default:
            break;
    }
}

bool semantic_analyze(ASTNode *root) {
    if (!root) return true;

    g_current_proto = NULL;
    g_loop_depth = 0;
    clear_protos();

    SymbolTable *st = symtab_new();
    check_stmt(root, st);
    symtab_free(st);
    clear_protos();

    return (ice_get_error_count() == 0);
}
