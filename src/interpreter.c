#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "interpreter.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
 * Variavel em memoria de execucao
 */
typedef struct VarEntry {
    char *name;
    Value value;
    struct VarEntry *next;
} VarEntry;

/*
 * Escopo de memoria de execucao
 */
typedef struct RuntimeScope {
    int level;
    VarEntry *vars;
    struct RuntimeScope *parent;
} RuntimeScope;

/*
 * Contexto global de execucao
 */
typedef struct {
    RuntimeScope *current_scope;
    int64_t signal_reg; /* registrador signal */
    FlowSignal flow;
    int eject_code;
    bool has_runtime_error;
} RuntimeContext;

static RuntimeContext g_ctx;

static RuntimeScope *rscope_new(RuntimeScope *parent) {
    RuntimeScope *s = (RuntimeScope *)calloc(1, sizeof(RuntimeScope));
    if (!s) {
        fprintf(stderr, "[GHOST::ICE_FAULT] Falha ao alocar memoria para escopo de execucao\n");
        exit(1);
    }
    s->parent = parent;
    s->level = parent ? parent->level + 1 : 0;
    s->vars = NULL;
    return s;
}

static void value_free(Value *v) {
    if (v && v->type == TYPE_STREAM && v->val_stream) {
        free(v->val_stream);
        v->val_stream = NULL;
    }
}

static void rscope_free(RuntimeScope *s) {
    if (!s) return;
    VarEntry *curr = s->vars;
    while (curr) {
        VarEntry *next = curr->next;
        free(curr->name);
        value_free(&curr->value);
        free(curr);
        curr = next;
    }
    free(s);
}

static void env_push_scope(void) {
    g_ctx.current_scope = rscope_new(g_ctx.current_scope);
}

static void env_pop_scope(void) {
    if (g_ctx.current_scope && g_ctx.current_scope->parent) {
        RuntimeScope *old = g_ctx.current_scope;
        g_ctx.current_scope = old->parent;
        rscope_free(old);
    }
}

static void env_set_local(const char *name, Value val) {
    /* Aloca ou sobrescreve no escopo atual */
    for (VarEntry *v = g_ctx.current_scope->vars; v; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            value_free(&v->value);
            v->value = val;
            return;
        }
    }
    VarEntry *new_var = (VarEntry *)calloc(1, sizeof(VarEntry));
    new_var->name = strdup(name);
    new_var->value = val;
    new_var->next = g_ctx.current_scope->vars;
    g_ctx.current_scope->vars = new_var;
}

static bool env_assign(const char *name, Value val) {
    /* Busca do escopo atual para cima e atualiza */
    for (RuntimeScope *s = g_ctx.current_scope; s; s = s->parent) {
        for (VarEntry *v = s->vars; v; v = v->next) {
            if (strcmp(v->name, name) == 0) {
                value_free(&v->value);
                v->value = val;
                return true;
            }
        }
    }
    return false;
}

static Value env_get(const char *name, int line, int col) {
    if (strcmp(name, "signal") == 0) {
        Value v;
        v.type = TYPE_CORE;
        v.val_core = g_ctx.signal_reg;
        return v;
    }

    for (RuntimeScope *s = g_ctx.current_scope; s; s = s->parent) {
        for (VarEntry *v = s->vars; v; v = v->next) {
            if (strcmp(v->name, name) == 0) {
                /* Retorna copia do valor */
                Value res = v->value;
                if (res.type == TYPE_STREAM && res.val_stream) {
                    res.val_stream = strdup(res.val_stream);
                }
                return res;
            }
        }
    }

    ice_error(ICE_RUNTIME, line, col, "identificador '%s' nao encontrado na memoria", name);
    g_ctx.has_runtime_error = true;
    Value def;
    def.type = TYPE_CORE;
    def.val_core = 0;
    return def;
}

static Value default_value_for_type(DataType t) {
    Value v;
    v.type = t;
    switch (t) {
        case TYPE_CORE:   v.val_core = 0; break;
        case TYPE_FLUX:   v.val_flux = 0.0; break;
        case TYPE_STATE:  v.val_state = false; break;
        case TYPE_STREAM: v.val_stream = strdup(""); break;
        default:          v.val_core = 0; break;
    }
    return v;
}

/*
 * Avaliacao recursiva de expressoes
 */
static Value eval_expr(ASTNode *node) {
    Value res;
    res.type = TYPE_CORE;
    res.val_core = 0;

    if (!node || g_ctx.has_runtime_error || g_ctx.flow != FLOW_NORMAL) {
        return res;
    }

    switch (node->type) {
        case AST_LITERAL_CORE:
            res.type = TYPE_CORE;
            res.val_core = node->literal_core;
            return res;

        case AST_LITERAL_FLUX:
            res.type = TYPE_FLUX;
            res.val_flux = node->literal_flux;
            return res;

        case AST_LITERAL_STATE:
            res.type = TYPE_STATE;
            res.val_state = node->literal_state;
            return res;

        case AST_LITERAL_STREAM:
            res.type = TYPE_STREAM;
            res.val_stream = strdup(node->literal_stream ? node->literal_stream : "");
            return res;

        case AST_ID:
            return env_get(node->id_name, node->line, node->col);

        case AST_SIGNAL:
            res.type = TYPE_CORE;
            res.val_core = g_ctx.signal_reg;
            return res;

        case AST_SHELL_EXPR: {
            Value cmd_val = eval_expr(node->shell_expr.cmd_expr);
            const char *cmd_str = cmd_val.val_stream ? cmd_val.val_stream : "";

            FILE *fp = popen(cmd_str, "r");
            value_free(&cmd_val);

            if (!fp) {
                res.type = TYPE_STREAM;
                res.val_stream = strdup("");
                g_ctx.signal_reg = 127;
                return res;
            }

            size_t cap = 512;
            size_t len = 0;
            char *buf = (char *)malloc(cap);
            if (!buf) {
                pclose(fp);
                res.type = TYPE_STREAM;
                res.val_stream = strdup("");
                return res;
            }

            char chunk[256];
            while (fgets(chunk, sizeof(chunk), fp)) {
                size_t clen = strlen(chunk);
                if (len + clen + 1 >= cap) {
                    cap = (cap * 2) + clen;
                    buf = (char *)realloc(buf, cap);
                }
                memcpy(buf + len, chunk, clen);
                len += clen;
                buf[len] = '\0';
            }

            int status = pclose(fp);
            if (WIFEXITED(status)) {
                g_ctx.signal_reg = WEXITSTATUS(status);
            } else {
                g_ctx.signal_reg = 0;
            }

            /* Remove quebra de linha final se houver (stream limpo) */
            if (len > 0 && buf[len - 1] == '\n') {
                buf[len - 1] = '\0';
                len--;
            }
            if (len > 0 && buf[len - 1] == '\r') {
                buf[len - 1] = '\0';
                len--;
            }

            res.type = TYPE_STREAM;
            res.val_stream = buf;
            return res;
        }

        case AST_CALL: {
            if (strcmp(node->call.name, "exists") == 0) {
                res.type = TYPE_STATE;
                if (node->call.args) {
                    Value arg_val = eval_expr(node->call.args->node);
                    if (arg_val.type == TYPE_STREAM && arg_val.val_stream) {
                        res.val_state = (access(arg_val.val_stream, F_OK) == 0);
                    } else {
                        res.val_state = false;
                    }
                    value_free(&arg_val);
                } else {
                    res.val_state = false;
                }
                return res;
            } else if (strcmp(node->call.name, "env") == 0) {
                res.type = TYPE_STREAM;
                if (node->call.args) {
                    Value arg_val = eval_expr(node->call.args->node);
                    if (arg_val.type == TYPE_STREAM && arg_val.val_stream) {
                        char *ev = getenv(arg_val.val_stream);
                        res.val_stream = strdup(ev ? ev : "");
                    } else {
                        res.val_stream = strdup("");
                    }
                    value_free(&arg_val);
                } else {
                    res.val_stream = strdup("");
                }
                return res;
            }
            break;
        }

        case AST_UNOP: {
            Value op_val = eval_expr(node->unop.operand);
            if (node->unop.op == UNOP_NOT) {
                res.type = TYPE_STATE;
                res.val_state = !op_val.val_state;
                return res;
            } else if (node->unop.op == UNOP_NEG) {
                if (op_val.type == TYPE_CORE) {
                    res.type = TYPE_CORE;
                    res.val_core = -op_val.val_core;
                    return res;
                } else if (op_val.type == TYPE_FLUX) {
                    res.type = TYPE_FLUX;
                    res.val_flux = -op_val.val_flux;
                    return res;
                }
            }
            break;
        }

        case AST_BINOP: {
            /* Curto-circuito para operadores logicos with e either */
            if (node->binop.op == BINOP_AND) {
                Value left = eval_expr(node->binop.left);
                res.type = TYPE_STATE;
                if (!left.val_state) {
                    res.val_state = false;
                    return res;
                }
                Value right = eval_expr(node->binop.right);
                res.val_state = right.val_state;
                return res;
            } else if (node->binop.op == BINOP_OR) {
                Value left = eval_expr(node->binop.left);
                res.type = TYPE_STATE;
                if (left.val_state) {
                    res.val_state = true;
                    return res;
                }
                Value right = eval_expr(node->binop.right);
                res.val_state = right.val_state;
                return res;
            }

            Value left = eval_expr(node->binop.left);
            Value right = eval_expr(node->binop.right);

            /* Concatenação de streams (&) */
            if (node->binop.op == BINOP_JOIN) {
                const char *s1 = left.val_stream ? left.val_stream : "";
                const char *s2 = right.val_stream ? right.val_stream : "";
                size_t l1 = strlen(s1);
                size_t l2 = strlen(s2);
                char *out = (char *)malloc(l1 + l2 + 1);
                memcpy(out, s1, l1);
                memcpy(out + l1, s2, l2);
                out[l1 + l2] = '\0';

                value_free(&left);
                value_free(&right);

                res.type = TYPE_STREAM;
                res.val_stream = out;
                return res;
            }

            /* Operacoes Aritmeticas */
            bool is_flux = (left.type == TYPE_FLUX || right.type == TYPE_FLUX);
            double fl = (left.type == TYPE_FLUX) ? left.val_flux : (double)left.val_core;
            double fr = (right.type == TYPE_FLUX) ? right.val_flux : (double)right.val_core;
            int64_t il = left.val_core;
            int64_t ir = right.val_core;

            switch (node->binop.op) {
                case BINOP_ADD:
                    if (is_flux) {
                        res.type = TYPE_FLUX;
                        res.val_flux = fl + fr;
                    } else {
                        res.type = TYPE_CORE;
                        res.val_core = il + ir;
                    }
                    break;

                case BINOP_SUB:
                    if (is_flux) {
                        res.type = TYPE_FLUX;
                        res.val_flux = fl - fr;
                    } else {
                        res.type = TYPE_CORE;
                        res.val_core = il - ir;
                    }
                    break;

                case BINOP_MUL:
                    if (is_flux) {
                        res.type = TYPE_FLUX;
                        res.val_flux = fl * fr;
                    } else {
                        res.type = TYPE_CORE;
                        res.val_core = il * ir;
                    }
                    break;

                case BINOP_DIV:
                    if (is_flux) {
                        if (fr == 0.0) {
                            ice_error(ICE_RUNTIME, node->line, node->col,
                                      "divisao aritmetica por zero (falha de barramento numerico)");
                            g_ctx.has_runtime_error = true;
                            res.type = TYPE_FLUX;
                            res.val_flux = 0.0;
                            return res;
                        }
                        res.type = TYPE_FLUX;
                        res.val_flux = fl / fr;
                    } else {
                        if (ir == 0) {
                            ice_error(ICE_RUNTIME, node->line, node->col,
                                      "divisao aritmetica por zero (falha de barramento numerico)");
                            g_ctx.has_runtime_error = true;
                            res.type = TYPE_CORE;
                            res.val_core = 0;
                            return res;
                        }
                        res.type = TYPE_CORE;
                        res.val_core = il / ir;
                    }
                    break;

                case BINOP_MOD:
                    if (ir == 0) {
                        ice_error(ICE_RUNTIME, node->line, node->col,
                                  "divisao aritmetica por zero no calculo de resto");
                        g_ctx.has_runtime_error = true;
                        res.type = TYPE_CORE;
                        res.val_core = 0;
                        return res;
                    }
                    res.type = TYPE_CORE;
                    res.val_core = il % ir;
                    break;

                case BINOP_LT:
                    res.type = TYPE_STATE;
                    res.val_state = is_flux ? (fl < fr) : (il < ir);
                    break;

                case BINOP_GT:
                    res.type = TYPE_STATE;
                    res.val_state = is_flux ? (fl > fr) : (il > ir);
                    break;

                case BINOP_LE:
                    res.type = TYPE_STATE;
                    res.val_state = is_flux ? (fl <= fr) : (il <= ir);
                    break;

                case BINOP_GE:
                    res.type = TYPE_STATE;
                    res.val_state = is_flux ? (fl >= fr) : (il >= ir);
                    break;

                case BINOP_EQ:
                    res.type = TYPE_STATE;
                    if (left.type == TYPE_STREAM && right.type == TYPE_STREAM) {
                        res.val_state = (strcmp(left.val_stream, right.val_stream) == 0);
                    } else if (left.type == TYPE_STATE && right.type == TYPE_STATE) {
                        res.val_state = (left.val_state == right.val_state);
                    } else {
                        res.val_state = is_flux ? (fl == fr) : (il == ir);
                    }
                    break;

                case BINOP_NEQ:
                    res.type = TYPE_STATE;
                    if (left.type == TYPE_STREAM && right.type == TYPE_STREAM) {
                        res.val_state = (strcmp(left.val_stream, right.val_stream) != 0);
                    } else if (left.type == TYPE_STATE && right.type == TYPE_STATE) {
                        res.val_state = (left.val_state != right.val_state);
                    } else {
                        res.val_state = is_flux ? (fl != fr) : (il != ir);
                    }
                    break;

                default:
                    break;
            }

            value_free(&left);
            value_free(&right);
            return res;
        }

        default:
            break;
    }

    return res;
}

/*
 * Execucao de comandos da AST
 */
static void exec_stmt(ASTNode *node) {
    if (!node || g_ctx.has_runtime_error || g_ctx.flow != FLOW_NORMAL) {
        return;
    }

    switch (node->type) {
        case AST_PROGRAM:
            for (ASTNodeList *l = node->program.stmts; l; l = l->next) {
                exec_stmt(l->node);
                if (g_ctx.flow != FLOW_NORMAL || g_ctx.has_runtime_error) {
                    break;
                }
            }
            break;

        case AST_BLOCK:
            env_push_scope();
            for (ASTNodeList *l = node->block.stmts; l; l = l->next) {
                exec_stmt(l->node);
                if (g_ctx.flow != FLOW_NORMAL || g_ctx.has_runtime_error) {
                    break;
                }
            }
            env_pop_scope();
            break;

        case AST_IMPLANT: {
            Value init_v;
            if (node->implant.init_expr) {
                init_v = eval_expr(node->implant.init_expr);
                /* Coerção core -> flux se necessario */
                if (node->implant.var_type == TYPE_FLUX && init_v.type == TYPE_CORE) {
                    double f = (double)init_v.val_core;
                    init_v.type = TYPE_FLUX;
                    init_v.val_flux = f;
                }
            } else {
                init_v = default_value_for_type(node->implant.var_type);
            }
            env_set_local(node->implant.name, init_v);
            break;
        }

        case AST_ASSIGN: {
            Value v = eval_expr(node->assign.expr);
            /* Busca simbolo para checar se precisa de coerção */
            /* Atualiza valor */
            env_assign(node->assign.name, v);
            break;
        }

        case AST_WHISPER: {
            for (ASTNodeList *l = node->whisper.args; l; l = l->next) {
                Value v = eval_expr(l->node);
                switch (v.type) {
                    case TYPE_CORE:
                        printf("%ld", (long)v.val_core);
                        break;
                    case TYPE_FLUX:
                        printf("%g", v.val_flux);
                        break;
                    case TYPE_STATE:
                        printf("%s", v.val_state ? "sync" : "void");
                        break;
                    case TYPE_STREAM:
                        printf("%s", v.val_stream ? v.val_stream : "");
                        break;
                    default:
                        break;
                }
                value_free(&v);
            }
            printf("\n");
            fflush(stdout);
            break;
        }

        case AST_WHEN: {
            Value cond_v = eval_expr(node->when_stmt.cond);
            if (cond_v.val_state) {
                exec_stmt(node->when_stmt.then_branch);
            } else if (node->when_stmt.else_branch) {
                exec_stmt(node->when_stmt.else_branch);
            }
            break;
        }

        case AST_CYCLE: {
            while (!g_ctx.has_runtime_error && g_ctx.flow == FLOW_NORMAL) {
                Value cond_v = eval_expr(node->cycle_stmt.cond);
                if (!cond_v.val_state) {
                    break;
                }
                exec_stmt(node->cycle_stmt.body);

                if (g_ctx.flow == FLOW_CLOAK) {
                    g_ctx.flow = FLOW_NORMAL; /* Quebra o laço */
                    break;
                } else if (g_ctx.flow == FLOW_BYPASS) {
                    g_ctx.flow = FLOW_NORMAL; /* Continua próxima iteração */
                } else if (g_ctx.flow == FLOW_EJECT) {
                    break;
                }
            }
            break;
        }

        case AST_CLOAK:
            g_ctx.flow = FLOW_CLOAK;
            break;

        case AST_BYPASS:
            g_ctx.flow = FLOW_BYPASS;
            break;

        case AST_SHELL_CMD: {
            Value cmd_v = eval_expr(node->shell_cmd.cmd_expr);
            const char *cmd = cmd_v.val_stream ? cmd_v.val_stream : "";
            int status = system(cmd);
            if (WIFEXITED(status)) {
                g_ctx.signal_reg = WEXITSTATUS(status);
            } else {
                g_ctx.signal_reg = 0;
            }
            value_free(&cmd_v);
            break;
        }

        case AST_EJECT: {
            int code = 0;
            if (node->eject_stmt.code_expr) {
                Value c = eval_expr(node->eject_stmt.code_expr);
                code = (int)c.val_core;
            }
            g_ctx.eject_code = code;
            g_ctx.flow = FLOW_EJECT;
            break;
        }

        default:
            break;
    }
}

int interpret(ASTNode *root) {
    if (!root) return 0;

    memset(&g_ctx, 0, sizeof(RuntimeContext));
    g_ctx.current_scope = rscope_new(NULL);
    g_ctx.signal_reg = 0;
    g_ctx.flow = FLOW_NORMAL;
    g_ctx.eject_code = 0;
    g_ctx.has_runtime_error = false;

    exec_stmt(root);

    int exit_code = g_ctx.eject_code;
    if (g_ctx.has_runtime_error) {
        exit_code = 1;
    }

    rscope_free(g_ctx.current_scope);
    g_ctx.current_scope = NULL;

    return exit_code;
}
