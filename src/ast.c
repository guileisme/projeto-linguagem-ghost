#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ASTNode *ast_alloc(ASTNodeType type, int line, int col) {
    ASTNode *n = (ASTNode *)calloc(1, sizeof(ASTNode));
    if (!n) {
        fprintf(stderr, "[GHOST::ICE_FAULT] Falha de alocacao de memoria para no da AST\n");
        exit(1);
    }
    n->type = type;
    n->expr_type = TYPE_UNKNOWN;
    n->line = line;
    n->col = col;
    return n;
}

ASTNode *ast_new_literal_core(int64_t val, int line, int col) {
    ASTNode *n = ast_alloc(AST_LITERAL_CORE, line, col);
    n->expr_type = TYPE_CORE;
    n->literal_core = val;
    return n;
}

ASTNode *ast_new_literal_flux(double val, int line, int col) {
    ASTNode *n = ast_alloc(AST_LITERAL_FLUX, line, col);
    n->expr_type = TYPE_FLUX;
    n->literal_flux = val;
    return n;
}

ASTNode *ast_new_literal_state(bool val, int line, int col) {
    ASTNode *n = ast_alloc(AST_LITERAL_STATE, line, col);
    n->expr_type = TYPE_STATE;
    n->literal_state = val;
    return n;
}

ASTNode *ast_new_literal_stream(char *val, int line, int col) {
    ASTNode *n = ast_alloc(AST_LITERAL_STREAM, line, col);
    n->expr_type = TYPE_STREAM;
    n->literal_stream = val;
    return n;
}

ASTNode *ast_new_id(char *name, int line, int col) {
    ASTNode *n = ast_alloc(AST_ID, line, col);
    n->id_name = name;
    return n;
}

ASTNode *ast_new_signal(int line, int col) {
    ASTNode *n = ast_alloc(AST_SIGNAL, line, col);
    n->expr_type = TYPE_CORE;
    return n;
}

ASTNode *ast_new_binop(BinaryOp op, ASTNode *left, ASTNode *right, int line, int col) {
    ASTNode *n = ast_alloc(AST_BINOP, line, col);
    n->binop.op = op;
    n->binop.left = left;
    n->binop.right = right;
    return n;
}

ASTNode *ast_new_unop(UnaryOp op, ASTNode *operand, int line, int col) {
    ASTNode *n = ast_alloc(AST_UNOP, line, col);
    n->unop.op = op;
    n->unop.operand = operand;
    return n;
}

ASTNode *ast_new_shell_expr(ASTNode *cmd_expr, int line, int col) {
    ASTNode *n = ast_alloc(AST_SHELL_EXPR, line, col);
    n->expr_type = TYPE_STREAM;
    n->shell_expr.cmd_expr = cmd_expr;
    return n;
}

ASTNode *ast_new_call(char *name, ASTNodeList *args, int line, int col) {
    ASTNode *n = ast_alloc(AST_CALL, line, col);
    n->call.name = name;
    n->call.args = args;
    return n;
}

ASTNode *ast_new_implant(char *name, DataType var_type, ASTNode *init_expr, int line, int col) {
    ASTNode *n = ast_alloc(AST_IMPLANT, line, col);
    n->implant.name = name;
    n->implant.var_type = var_type;
    n->implant.init_expr = init_expr;
    return n;
}

ASTNode *ast_new_assign(char *name, ASTNode *expr, int line, int col) {
    ASTNode *n = ast_alloc(AST_ASSIGN, line, col);
    n->assign.name = name;
    n->assign.expr = expr;
    return n;
}

ASTNode *ast_new_whisper(ASTNodeList *args, int line, int col) {
    ASTNode *n = ast_alloc(AST_WHISPER, line, col);
    n->whisper.args = args;
    return n;
}

ASTNode *ast_new_scan(char *name, int line, int col) {
    ASTNode *n = ast_alloc(AST_SCAN, line, col);
    n->scan.name = name;
    return n;
}

ASTNode *ast_new_when(ASTNode *cond, ASTNode *then_branch, ASTNode *else_branch, int line, int col) {
    ASTNode *n = ast_alloc(AST_WHEN, line, col);
    n->when_stmt.cond = cond;
    n->when_stmt.then_branch = then_branch;
    n->when_stmt.else_branch = else_branch;
    return n;
}

ASTNode *ast_new_cycle(ASTNode *cond, ASTNode *body, int line, int col) {
    ASTNode *n = ast_alloc(AST_CYCLE, line, col);
    n->cycle_stmt.cond = cond;
    n->cycle_stmt.body = body;
    return n;
}

ASTNode *ast_new_sweep(char *var_name, ASTNode *from_expr, ASTNode *to_expr, ASTNode *body, int line, int col) {
    ASTNode *n = ast_alloc(AST_SWEEP, line, col);
    n->sweep_stmt.var_name = var_name;
    n->sweep_stmt.from_expr = from_expr;
    n->sweep_stmt.to_expr = to_expr;
    n->sweep_stmt.body = body;
    return n;
}

ASTNode *ast_new_route(char *var_name, ASTNode *expr, ASTNode *body, int line, int col) {
    ASTNode *n = ast_alloc(AST_ROUTE, line, col);
    n->route_stmt.var_name = var_name;
    n->route_stmt.expr = expr;
    n->route_stmt.body = body;
    return n;
}

ASTNode *ast_new_cloak(int line, int col) {
    return ast_alloc(AST_CLOAK, line, col);
}

ASTNode *ast_new_bypass(int line, int col) {
    return ast_alloc(AST_BYPASS, line, col);
}

ASTNode *ast_new_shell_cmd(ASTNode *cmd_expr, int line, int col) {
    ASTNode *n = ast_alloc(AST_SHELL_CMD, line, col);
    n->shell_cmd.cmd_expr = cmd_expr;
    return n;
}

ASTNode *ast_new_eject(ASTNode *code_expr, int line, int col) {
    ASTNode *n = ast_alloc(AST_EJECT, line, col);
    n->eject_stmt.code_expr = code_expr;
    return n;
}

ASTNode *ast_new_block(ASTNodeList *stmts, int line, int col) {
    ASTNode *n = ast_alloc(AST_BLOCK, line, col);
    n->block.stmts = stmts;
    return n;
}

ASTNode *ast_new_program(ASTNodeList *stmts, int line, int col) {
    ASTNode *n = ast_alloc(AST_PROGRAM, line, col);
    n->program.stmts = stmts;
    return n;
}

ASTNode *ast_new_protocol(char *name, ParamNode *params, DataType return_type, ASTNode *body, int line, int col) {
    ASTNode *n = ast_alloc(AST_PROTOCOL, line, col);
    n->protocol.name = name;
    n->protocol.params = params;
    n->protocol.return_type = return_type;
    n->protocol.body = body;
    return n;
}

ASTNode *ast_new_yield(ASTNode *expr, int line, int col) {
    ASTNode *n = ast_alloc(AST_YIELD, line, col);
    n->yield_stmt.expr = expr;
    return n;
}

ASTNode *ast_new_exec(ASTNode *call_node, int line, int col) {
    ASTNode *n = ast_alloc(AST_EXEC, line, col);
    n->unop.operand = call_node;
    return n;
}

/* Listas Encadeadas */
ASTNodeList *ast_list_new(ASTNode *node) {
    if (!node) return NULL;
    ASTNodeList *l = (ASTNodeList *)malloc(sizeof(ASTNodeList));
    if (!l) {
        fprintf(stderr, "[GHOST::ICE_FAULT] Falha de alocacao de lista da AST\n");
        exit(1);
    }
    l->node = node;
    l->next = NULL;
    return l;
}

ASTNodeList *ast_list_append(ASTNodeList *list, ASTNode *node) {
    if (!node) return list;
    if (!list) return ast_list_new(node);
    ASTNodeList *curr = list;
    while (curr->next) {
        curr = curr->next;
    }
    curr->next = ast_list_new(node);
    return list;
}

ParamNode *param_list_new(char *name, DataType type) {
    ParamNode *p = (ParamNode *)malloc(sizeof(ParamNode));
    if (!p) {
        fprintf(stderr, "[GHOST::ICE_FAULT] Falha de alocacao de lista de parametros\n");
        exit(1);
    }
    p->name = name;
    p->type = type;
    p->next = NULL;
    return p;
}

ParamNode *param_list_append(ParamNode *list, char *name, DataType type) {
    if (!list) return param_list_new(name, type);
    ParamNode *curr = list;
    while (curr->next) {
        curr = curr->next;
    }
    curr->next = param_list_new(name, type);
    return list;
}

const char *binop_to_string(BinaryOp op) {
    switch (op) {
        case BINOP_ADD:  return "+";
        case BINOP_SUB:  return "-";
        case BINOP_MUL:  return "*";
        case BINOP_DIV:  return "/";
        case BINOP_MOD:  return "%";
        case BINOP_JOIN: return "&";
        case BINOP_EQ:   return "=";
        case BINOP_NEQ:  return "=/=";
        case BINOP_LT:   return "<";
        case BINOP_GT:   return ">";
        case BINOP_LE:   return "<=";
        case BINOP_GE:   return ">=";
        case BINOP_AND:  return "with";
        case BINOP_OR:   return "either";
        default:         return "?";
    }
}

const char *unop_to_string(UnaryOp op) {
    switch (op) {
        case UNOP_NOT: return "never";
        case UNOP_NEG: return "-";
        default:       return "?";
    }
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void ast_print(ASTNode *node, int indent) {
    if (!node) return;

    print_indent(indent);

    switch (node->type) {
        case AST_PROGRAM:
            printf("Program (L%d:C%d)\n", node->line, node->col);
            for (ASTNodeList *l = node->program.stmts; l; l = l->next) {
                ast_print(l->node, indent + 1);
            }
            break;

        case AST_BLOCK:
            printf("Block [dive ... surface] (L%d:C%d)\n", node->line, node->col);
            for (ASTNodeList *l = node->block.stmts; l; l = l->next) {
                ast_print(l->node, indent + 1);
            }
            break;

        case AST_IMPLANT:
            printf("Implant: %s : %s (L%d:C%d)\n",
                   node->implant.name, type_to_string(node->implant.var_type),
                   node->line, node->col);
            if (node->implant.init_expr) {
                print_indent(indent + 1);
                printf("InitExpr (<~):\n");
                ast_print(node->implant.init_expr, indent + 2);
            }
            break;

        case AST_ASSIGN:
            printf("Assign (<~): %s (L%d:C%d)\n", node->assign.name, node->line, node->col);
            ast_print(node->assign.expr, indent + 1);
            break;

        case AST_WHISPER:
            printf("Whisper [saida stdout] (L%d:C%d)\n", node->line, node->col);
            for (ASTNodeList *l = node->whisper.args; l; l = l->next) {
                ast_print(l->node, indent + 1);
            }
            break;

        case AST_SCAN:
            printf("Scan [entrada stdin]: %s (L%d:C%d)\n", node->scan.name, node->line, node->col);
            break;

        case AST_WHEN:
            printf("When (L%d:C%d)\n", node->line, node->col);
            print_indent(indent + 1);
            printf("Cond:\n");
            ast_print(node->when_stmt.cond, indent + 2);
            print_indent(indent + 1);
            printf("ThenBranch:\n");
            ast_print(node->when_stmt.then_branch, indent + 2);
            if (node->when_stmt.else_branch) {
                print_indent(indent + 1);
                printf("OtherwiseBranch:\n");
                ast_print(node->when_stmt.else_branch, indent + 2);
            }
            break;

        case AST_CYCLE:
            printf("Cycle [while loop] (L%d:C%d)\n", node->line, node->col);
            print_indent(indent + 1);
            printf("Cond:\n");
            ast_print(node->cycle_stmt.cond, indent + 2);
            print_indent(indent + 1);
            printf("Body:\n");
            ast_print(node->cycle_stmt.body, indent + 2);
            break;

        case AST_SWEEP:
            printf("Sweep [for loop]: %s (L%d:C%d)\n", node->sweep_stmt.var_name, node->line, node->col);
            print_indent(indent + 1);
            printf("From:\n");
            ast_print(node->sweep_stmt.from_expr, indent + 2);
            print_indent(indent + 1);
            printf("To:\n");
            ast_print(node->sweep_stmt.to_expr, indent + 2);
            print_indent(indent + 1);
            printf("Body:\n");
            ast_print(node->sweep_stmt.body, indent + 2);
            break;

        case AST_ROUTE:
            printf("Route [line stream loop]: %s (L%d:C%d)\n", node->route_stmt.var_name, node->line, node->col);
            print_indent(indent + 1);
            printf("InExpr:\n");
            ast_print(node->route_stmt.expr, indent + 2);
            print_indent(indent + 1);
            printf("Body:\n");
            ast_print(node->route_stmt.body, indent + 2);
            break;

        case AST_CLOAK:
            printf("Cloak [break] (L%d:C%d)\n", node->line, node->col);
            break;

        case AST_BYPASS:
            printf("Bypass [continue] (L%d:C%d)\n", node->line, node->col);
            break;

        case AST_SHELL_CMD:
            printf("ShellCmd (L%d:C%d)\n", node->line, node->col);
            ast_print(node->shell_cmd.cmd_expr, indent + 1);
            break;

        case AST_SHELL_EXPR:
            printf("ShellExpr (L%d:C%d)\n", node->line, node->col);
            ast_print(node->shell_expr.cmd_expr, indent + 1);
            break;

        case AST_EJECT:
            printf("Eject [emergency disconnect] (L%d:C%d)\n", node->line, node->col);
            if (node->eject_stmt.code_expr) {
                ast_print(node->eject_stmt.code_expr, indent + 1);
            }
            break;

        case AST_PROTOCOL:
            printf("Protocol: %s returns %s (L%d:C%d)\n",
                   node->protocol.name, type_to_string(node->protocol.return_type),
                   node->line, node->col);
            if (node->protocol.params) {
                print_indent(indent + 1);
                printf("Params:\n");
                for (ParamNode *p = node->protocol.params; p; p = p->next) {
                    print_indent(indent + 2);
                    printf("%s : %s\n", p->name, type_to_string(p->type));
                }
            }
            print_indent(indent + 1);
            printf("Body:\n");
            ast_print(node->protocol.body, indent + 2);
            break;

        case AST_YIELD:
            printf("Yield [return] (L%d:C%d)\n", node->line, node->col);
            if (node->yield_stmt.expr) {
                ast_print(node->yield_stmt.expr, indent + 1);
            }
            break;

        case AST_EXEC:
            printf("Exec (L%d:C%d)\n", node->line, node->col);
            ast_print(node->unop.operand, indent + 1);
            break;

        case AST_BINOP:
            printf("BinOp (%s) (L%d:C%d)\n", binop_to_string(node->binop.op), node->line, node->col);
            ast_print(node->binop.left, indent + 1);
            ast_print(node->binop.right, indent + 1);
            break;

        case AST_UNOP:
            printf("UnOp (%s) (L%d:C%d)\n", unop_to_string(node->unop.op), node->line, node->col);
            ast_print(node->unop.operand, indent + 1);
            break;

        case AST_CALL:
            printf("Call: %s() (L%d:C%d)\n", node->call.name, node->line, node->col);
            for (ASTNodeList *l = node->call.args; l; l = l->next) {
                ast_print(l->node, indent + 1);
            }
            break;

        case AST_LITERAL_CORE:
            printf("Literal Core: %ld (L%d:C%d)\n", (long)node->literal_core, node->line, node->col);
            break;

        case AST_LITERAL_FLUX:
            printf("Literal Flux: %f (L%d:C%d)\n", node->literal_flux, node->line, node->col);
            break;

        case AST_LITERAL_STATE:
            printf("Literal State: %s (L%d:C%d)\n",
                   node->literal_state ? "sync (true)" : "void (false)",
                   node->line, node->col);
            break;

        case AST_LITERAL_STREAM:
            printf("Literal Stream: \"%s\" (L%d:C%d)\n",
                   node->literal_stream ? node->literal_stream : "",
                   node->line, node->col);
            break;

        case AST_ID:
            printf("Id: %s (L%d:C%d)\n", node->id_name, node->line, node->col);
            break;

        case AST_SIGNAL:
            printf("Signal [terminal exit code] (L%d:C%d)\n", node->line, node->col);
            break;
    }
}

static void free_ast_list(ASTNodeList *list) {
    while (list) {
        ASTNodeList *next = list->next;
        ast_free(list->node);
        free(list);
        list = next;
    }
}

static void free_param_list(ParamNode *list) {
    while (list) {
        ParamNode *next = list->next;
        free(list->name);
        free(list);
        list = next;
    }
}

void ast_free(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            free_ast_list(node->program.stmts);
            break;
        case AST_BLOCK:
            free_ast_list(node->block.stmts);
            break;
        case AST_IMPLANT:
            free(node->implant.name);
            ast_free(node->implant.init_expr);
            break;
        case AST_ASSIGN:
            free(node->assign.name);
            ast_free(node->assign.expr);
            break;
        case AST_WHISPER:
            free_ast_list(node->whisper.args);
            break;
        case AST_SCAN:
            free(node->scan.name);
            break;
        case AST_WHEN:
            ast_free(node->when_stmt.cond);
            ast_free(node->when_stmt.then_branch);
            ast_free(node->when_stmt.else_branch);
            break;
        case AST_CYCLE:
            ast_free(node->cycle_stmt.cond);
            ast_free(node->cycle_stmt.body);
            break;
        case AST_SWEEP:
            free(node->sweep_stmt.var_name);
            ast_free(node->sweep_stmt.from_expr);
            ast_free(node->sweep_stmt.to_expr);
            ast_free(node->sweep_stmt.body);
            break;
        case AST_ROUTE:
            free(node->route_stmt.var_name);
            ast_free(node->route_stmt.expr);
            ast_free(node->route_stmt.body);
            break;
        case AST_CLOAK:
        case AST_BYPASS:
        case AST_SIGNAL:
            break;
        case AST_SHELL_CMD:
            ast_free(node->shell_cmd.cmd_expr);
            break;
        case AST_SHELL_EXPR:
            ast_free(node->shell_expr.cmd_expr);
            break;
        case AST_EJECT:
            ast_free(node->eject_stmt.code_expr);
            break;
        case AST_PROTOCOL:
            free(node->protocol.name);
            free_param_list(node->protocol.params);
            ast_free(node->protocol.body);
            break;
        case AST_YIELD:
            ast_free(node->yield_stmt.expr);
            break;
        case AST_EXEC:
            ast_free(node->unop.operand);
            break;
        case AST_BINOP:
            ast_free(node->binop.left);
            ast_free(node->binop.right);
            break;
        case AST_UNOP:
            ast_free(node->unop.operand);
            break;
        case AST_CALL:
            free(node->call.name);
            free_ast_list(node->call.args);
            break;
        case AST_LITERAL_CORE:
        case AST_LITERAL_FLUX:
        case AST_LITERAL_STATE:
            break;
        case AST_LITERAL_STREAM:
            free(node->literal_stream);
            break;
        case AST_ID:
            free(node->id_name);
            break;
    }

    free(node);
}
