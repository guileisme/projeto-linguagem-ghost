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
    RuntimeScope *global_scope; /* escopo raiz: protocolos enxergam so ele */
    int64_t signal_reg; /* registrador signal */
    FlowSignal flow;
    int eject_code;
    bool has_runtime_error;
    Value ret_value;    /* valor devolvido por 'yield' */
    int call_depth;     /* profundidade de chamadas de protocolos */
} RuntimeContext;

static RuntimeContext g_ctx;

/* Argumentos do script, acessados por arg(n) */
static int g_script_argc = 0;
static char **g_script_argv = NULL;

void interpret_set_args(int argc, char **argv) {
    g_script_argc = argc;
    g_script_argv = argv;
}

#define MAX_CALL_DEPTH 500

/* Tabela de protocolos declarados (preenchida antes da execucao) */
typedef struct ProtoEntry {
    ASTNode *node;
    struct ProtoEntry *next;
} ProtoEntry;

static ProtoEntry *g_protocols = NULL;

static void proto_register(ASTNode *node) {
    ProtoEntry *e = (ProtoEntry *)calloc(1, sizeof(ProtoEntry));
    e->node = node;
    e->next = g_protocols;
    g_protocols = e;
}

static ASTNode *proto_find(const char *name) {
    for (ProtoEntry *e = g_protocols; e; e = e->next) {
        if (strcmp(e->node->protocol.name, name) == 0) {
            return e->node;
        }
    }
    return NULL;
}

static void proto_clear(void) {
    ProtoEntry *e = g_protocols;
    while (e) {
        ProtoEntry *next = e->next;
        free(e);
        e = next;
    }
    g_protocols = NULL;
}

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

static VarEntry *env_find(const char *name) {
    for (RuntimeScope *s = g_ctx.current_scope; s; s = s->parent) {
        for (VarEntry *v = s->vars; v; v = v->next) {
            if (strcmp(v->name, name) == 0) {
                return v;
            }
        }
    }
    return NULL;
}

static bool env_assign(const char *name, Value val) {
    /* Busca do escopo atual para cima e atualiza */
    VarEntry *v = env_find(name);
    if (!v) return false;

    /* Promocao implicita core -> flux, como na declaracao */
    if (v->value.type == TYPE_FLUX && val.type == TYPE_CORE) {
        double f = (double)val.val_core;
        val.type = TYPE_FLUX;
        val.val_flux = f;
    }
    value_free(&v->value);
    v->value = val;
    return true;
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

static Value eval_expr(ASTNode *node);
static void exec_stmt(ASTNode *node);

/*
 * Chamada de protocolo definido pelo usuario.
 * 1) avalia os argumentos no escopo de quem chamou;
 * 2) cria um escopo novo cujo pai e o escopo global (o protocolo nao enxerga
 *    as variaveis locais de quem chamou);
 * 3) liga cada parametro ao valor do argumento e executa o corpo;
 * 4) se o corpo executou 'yield', devolve o valor guardado em ret_value.
 */
static Value call_protocol(ASTNode *proto, ASTNodeList *args, int line, int col) {
    Value result;
    result.type = TYPE_CORE;
    result.val_core = 0;

    if (g_ctx.call_depth >= MAX_CALL_DEPTH) {
        ice_error(ICE_RUNTIME, line, col,
                  "limite de chamadas aninhadas excedido no protocolo '%s' (recursao infinita?)",
                  proto->protocol.name);
        g_ctx.has_runtime_error = true;
        return result;
    }

    /* 1) argumentos, avaliados no escopo de quem chamou */
    Value vals[16];
    int n = 0;
    for (ASTNodeList *a = args; a && n < 16; a = a->next) {
        vals[n++] = eval_expr(a->node);
    }
    if (g_ctx.has_runtime_error) {
        for (int i = 0; i < n; i++) value_free(&vals[i]);
        return result;
    }

    /* 2) escopo do protocolo, filho do escopo global */
    RuntimeScope *caller_scope = g_ctx.current_scope;
    RuntimeScope *call_scope = rscope_new(g_ctx.global_scope);
    g_ctx.current_scope = call_scope;

    /* 3) parametros */
    int i = 0;
    for (ParamNode *p = proto->protocol.params; p && i < n; p = p->next, i++) {
        Value v = vals[i];
        if (p->type == TYPE_FLUX && v.type == TYPE_CORE) {
            double f = (double)v.val_core;
            v.type = TYPE_FLUX;
            v.val_flux = f;
        }
        env_set_local(p->name, v);
    }

    g_ctx.call_depth++;
    exec_stmt(proto->protocol.body);
    g_ctx.call_depth--;

    /* 4) retorno */
    if (g_ctx.flow == FLOW_YIELD) {
        g_ctx.flow = FLOW_NORMAL;
        result = g_ctx.ret_value;
        g_ctx.ret_value.type = TYPE_CORE;
        g_ctx.ret_value.val_core = 0;
    } else if (g_ctx.flow == FLOW_CLOAK || g_ctx.flow == FLOW_BYPASS) {
        g_ctx.flow = FLOW_NORMAL; /* cloak/bypass nao atravessam protocolos */
    }
    /* FLOW_EJECT continua propagando ate o fim do programa */

    rscope_free(call_scope);
    g_ctx.current_scope = caller_scope;
    return result;
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
            } else if (strcmp(node->call.name, "arg") == 0) {
                /* arg(n): n-esimo argumento da linha de comando do script (1 = primeiro) */
                res.type = TYPE_STREAM;
                res.val_stream = strdup("");
                if (node->call.args) {
                    Value idx = eval_expr(node->call.args->node);
                    if (idx.type == TYPE_CORE && idx.val_core >= 1 &&
                        idx.val_core <= g_script_argc && g_script_argv) {
                        free(res.val_stream);
                        res.val_stream = strdup(g_script_argv[idx.val_core - 1]);
                    }
                }
                return res;
            }

            /* Protocolo definido pelo usuario */
            ASTNode *proto = proto_find(node->call.name);
            if (proto) {
                return call_protocol(proto, node->call.args, node->line, node->col);
            }
            ice_error(ICE_RUNTIME, node->line, node->col,
                      "protocolo '%s' nao definido", node->call.name);
            g_ctx.has_runtime_error = true;
            return res;
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
            /* Primeiro registra os protocolos: assim podem ser chamados
             * mesmo antes do ponto em que foram declarados no arquivo. */
            for (ASTNodeList *l = node->program.stmts; l; l = l->next) {
                if (l->node->type == AST_PROTOCOL) {
                    proto_register(l->node);
                }
            }
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
            /* Avalia todos os argumentos antes de imprimir: se algum falhar em
             * tempo de execucao, nada e impresso (evita saida parcial/falsa). */
            Value vals[32];
            int nvals = 0;
            for (ASTNodeList *l = node->whisper.args; l && nvals < 32; l = l->next) {
                vals[nvals++] = eval_expr(l->node);
            }
            if (g_ctx.has_runtime_error || g_ctx.flow != FLOW_NORMAL) {
                for (int i = 0; i < nvals; i++) value_free(&vals[i]);
                break;
            }
            for (int i = 0; i < nvals; i++) {
                Value v = vals[i];
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

        case AST_PROTOCOL:
            /* Declaracao: ja registrada no inicio do programa; nada a executar aqui */
            break;

        case AST_EXEC: {
            /* exec chamada! : executa o protocolo e descarta o valor devolvido */
            Value v = eval_expr(node->unop.operand);
            value_free(&v);
            break;
        }

        case AST_YIELD: {
            if (node->yield_stmt.expr) {
                Value v = eval_expr(node->yield_stmt.expr);
                if (g_ctx.has_runtime_error) {
                    value_free(&v);
                    break;
                }
                value_free(&g_ctx.ret_value);
                g_ctx.ret_value = v;
            }
            g_ctx.flow = FLOW_YIELD;
            break;
        }

        case AST_SWEEP: {
            /* sweep i from A to B : conta de A ate B, inclusive, de 1 em 1 */
            Value vf = eval_expr(node->sweep_stmt.from_expr);
            Value vt = eval_expr(node->sweep_stmt.to_expr);
            if (g_ctx.has_runtime_error) break;

            env_push_scope();
            for (int64_t i = vf.val_core; i <= vt.val_core; i++) {
                Value iv;
                iv.type = TYPE_CORE;
                iv.val_core = i;
                env_set_local(node->sweep_stmt.var_name, iv);

                exec_stmt(node->sweep_stmt.body);

                if (g_ctx.flow == FLOW_CLOAK) {
                    g_ctx.flow = FLOW_NORMAL;
                    break;
                } else if (g_ctx.flow == FLOW_BYPASS) {
                    g_ctx.flow = FLOW_NORMAL;
                } else if (g_ctx.flow != FLOW_NORMAL || g_ctx.has_runtime_error) {
                    break; /* eject, yield ou erro: sai do laco */
                }
            }
            env_pop_scope();
            break;
        }

        case AST_ROUTE: {
            /* route item in fluxo : uma iteracao por linha do stream */
            Value src = eval_expr(node->route_stmt.expr);
            if (g_ctx.has_runtime_error) {
                value_free(&src);
                break;
            }

            char *text = strdup(src.val_stream ? src.val_stream : "");
            value_free(&src);

            env_push_scope();
            if (text[0] != '\0') {
                char *cursor = text;
                while (cursor) {
                    char *nl = strchr(cursor, '\n');
                    if (nl) *nl = '\0';

                    size_t len = strlen(cursor);
                    if (len > 0 && cursor[len - 1] == '\r') cursor[len - 1] = '\0';

                    Value lv;
                    lv.type = TYPE_STREAM;
                    lv.val_stream = strdup(cursor);
                    env_set_local(node->route_stmt.var_name, lv);

                    exec_stmt(node->route_stmt.body);

                    bool stop = false;
                    if (g_ctx.flow == FLOW_CLOAK) {
                        g_ctx.flow = FLOW_NORMAL;
                        stop = true;
                    } else if (g_ctx.flow == FLOW_BYPASS) {
                        g_ctx.flow = FLOW_NORMAL;
                    } else if (g_ctx.flow != FLOW_NORMAL || g_ctx.has_runtime_error) {
                        stop = true;
                    }
                    if (stop) break;

                    cursor = nl ? nl + 1 : NULL;
                }
            }
            env_pop_scope();
            free(text);
            break;
        }

        case AST_SCAN: {
            /* scan x! : le uma linha da entrada padrao e converte para o tipo de x */
            VarEntry *var = env_find(node->scan.name);
            if (!var) {
                ice_error(ICE_RUNTIME, node->line, node->col,
                          "identificador '%s' nao encontrado na memoria", node->scan.name);
                g_ctx.has_runtime_error = true;
                break;
            }

            char line[1024];
            if (!fgets(line, sizeof(line), stdin)) {
                ice_error(ICE_RUNTIME, node->line, node->col,
                          "fim da entrada padrao ao executar 'scan %s'", node->scan.name);
                g_ctx.has_runtime_error = true;
                break;
            }
            size_t len = strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                line[--len] = '\0';
            }

            Value nv;
            nv.type = var->value.type;
            char *end = NULL;
            switch (var->value.type) {
                case TYPE_CORE:
                    nv.val_core = strtoll(line, &end, 10);
                    if (end == line || *end != '\0') {
                        ice_error(ICE_RUNTIME, node->line, node->col,
                                  "entrada '%s' invalida para o tipo 'core'", line);
                        g_ctx.has_runtime_error = true;
                    }
                    break;
                case TYPE_FLUX:
                    nv.val_flux = strtod(line, &end);
                    if (end == line || *end != '\0') {
                        ice_error(ICE_RUNTIME, node->line, node->col,
                                  "entrada '%s' invalida para o tipo 'flux'", line);
                        g_ctx.has_runtime_error = true;
                    }
                    break;
                case TYPE_STATE:
                    if (strcmp(line, "sync") == 0) {
                        nv.val_state = true;
                    } else if (strcmp(line, "void") == 0) {
                        nv.val_state = false;
                    } else {
                        ice_error(ICE_RUNTIME, node->line, node->col,
                                  "entrada '%s' invalida para o tipo 'state' (use sync ou void)", line);
                        g_ctx.has_runtime_error = true;
                    }
                    break;
                default:
                    nv.type = TYPE_STREAM;
                    nv.val_stream = strdup(line);
                    break;
            }
            if (!g_ctx.has_runtime_error) {
                value_free(&var->value);
                var->value = nv;
            }
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
    g_ctx.global_scope = g_ctx.current_scope;
    g_ctx.ret_value.type = TYPE_CORE;
    g_ctx.ret_value.val_core = 0;
    g_ctx.signal_reg = 0;
    g_ctx.flow = FLOW_NORMAL;
    g_ctx.eject_code = 0;
    g_ctx.has_runtime_error = false;

    exec_stmt(root);

    int exit_code = g_ctx.eject_code;
    if (g_ctx.has_runtime_error) {
        exit_code = 1;
    }

    rscope_free(g_ctx.global_scope);
    g_ctx.current_scope = NULL;
    g_ctx.global_scope = NULL;
    proto_clear();

    return exit_code;
}
