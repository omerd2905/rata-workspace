#include "engines/expr.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum ExprNodeKind {
    EXPR_NUM = 0,
    EXPR_VAR = 1,
    EXPR_UNARY_NEG = 2,
    EXPR_ADD = 3,
    EXPR_SUB = 4,
    EXPR_MUL = 5,
    EXPR_DIV = 6,
    EXPR_POW = 7,
    EXPR_LT = 8,
    EXPR_LE = 9,
    EXPR_GT = 10,
    EXPR_GE = 11,
    EXPR_EQ = 12,
    EXPR_NE = 13,
    EXPR_CALL = 14
} ExprNodeKind;

typedef enum ExprFnId {
    FN_SIN = 0,
    FN_COS,
    FN_TAN,
    FN_ASIN,
    FN_ACOS,
    FN_ATAN,
    FN_ATAN2,
    FN_SINH,
    FN_COSH,
    FN_TANH,
    FN_EXP,
    FN_LN,
    FN_LOG10,
    FN_LOG2,
    FN_SQRT,
    FN_ABS,
    FN_FLOOR,
    FN_CEIL,
    FN_ROUND,
    FN_MIN,
    FN_MAX,
    FN_MOD,
    FN_DELTA,
    FN_STEP,
    FN_RAMP,
    FN_RECT,
    FN_TRI,
    FN_SINC,
    FN_SGN,
    FN_IF
} ExprFnId;

struct RataExprNode {
    ExprNodeKind kind;
    double number;
    ExprFnId fn_id;
    char ident[32];
    int32_t a;
    int32_t b;
    int32_t c;
};

typedef struct ExprParser {
    const char *src;
    size_t len;
    size_t pos;
    size_t depth;
    RataExprNode *nodes;
    size_t node_count;
    size_t node_cap;
    RataExprError *err;
    bool has_error;
} ExprParser;

static void parser_error(ExprParser *p, const char *msg) {
    if (p->has_error) {
        return;
    }
    p->has_error = true;
    if (p->err != NULL) {
        p->err->col = p->pos;
        snprintf(p->err->message, sizeof(p->err->message), "%s", msg);
    }
}

static void skip_ws(ExprParser *p) {
    while (p->pos < p->len) {
        unsigned char ch = (unsigned char)p->src[p->pos];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            p->pos++;
        } else {
            break;
        }
    }
}

static char peek_ch(const ExprParser *p) {
    if (p->pos >= p->len) {
        return '\0';
    }
    return p->src[p->pos];
}

static int32_t push_node(ExprParser *p, const RataExprNode *node) {
    if (p->node_count >= p->node_cap) {
        parser_error(p, "expression too complex (node limit reached)");
        return -1;
    }
    int32_t idx = (int32_t)p->node_count;
    p->nodes[p->node_count++] = *node;
    return idx;
}

static bool lookup_fn(const char *name, ExprFnId *id_out, int *arity_out) {
    typedef struct FnEntry {
        const char *name;
        ExprFnId id;
        int arity;
    } FnEntry;
    static const FnEntry table[] = {
        {"sin", FN_SIN, 1},     {"cos", FN_COS, 1},     {"tan", FN_TAN, 1},
        {"asin", FN_ASIN, 1},   {"acos", FN_ACOS, 1},   {"atan", FN_ATAN, 1},
        {"atan2", FN_ATAN2, 2}, {"sinh", FN_SINH, 1},   {"cosh", FN_COSH, 1},
        {"tanh", FN_TANH, 1},   {"exp", FN_EXP, 1},     {"ln", FN_LN, 1},
        {"log", FN_LN, 1},      {"log10", FN_LOG10, 1}, {"log2", FN_LOG2, 1},
        {"sqrt", FN_SQRT, 1},   {"abs", FN_ABS, 1},     {"floor", FN_FLOOR, 1},
        {"ceil", FN_CEIL, 1},   {"round", FN_ROUND, 1}, {"min", FN_MIN, 2},
        {"max", FN_MAX, 2},     {"mod", FN_MOD, 2},     {"delta", FN_DELTA, 1},
        {"d", FN_DELTA, 1},     {"u", FN_STEP, 1},      {"step", FN_STEP, 1},
        {"ramp", FN_RAMP, 1},   {"r", FN_RAMP, 1},      {"rect", FN_RECT, 1},
        {"tri", FN_TRI, 1},     {"sinc", FN_SINC, 1},   {"sgn", FN_SGN, 1},
        {"sign", FN_SGN, 1},    {"if", FN_IF, 3}
    };
    size_t count = sizeof(table) / sizeof(table[0]);
    for (size_t i = 0u; i < count; ++i) {
        if (strcmp(name, table[i].name) == 0) {
            *id_out = table[i].id;
            *arity_out = table[i].arity;
            return true;
        }
    }
    return false;
}

static int32_t parse_comparison(ExprParser *p);

static int32_t parse_primary(ExprParser *p) {
    skip_ws(p);
    if (p->has_error) {
        return -1;
    }
    if (p->depth >= RATA_EXPR_MAX_DEPTH) {
        parser_error(p, "maximum nesting depth exceeded");
        return -1;
    }

    char ch = peek_ch(p);
    if (ch == '\0') {
        parser_error(p, "unexpected end of expression");
        return -1;
    }

    if (ch == '(') {
        p->pos++;
        p->depth++;
        int32_t inner = parse_comparison(p);
        p->depth--;
        if (p->has_error) {
            return -1;
        }
        skip_ws(p);
        if (peek_ch(p) != ')') {
            parser_error(p, "missing closing ')'");
            return -1;
        }
        p->pos++;
        return inner;
    }

    /* Numeric literal (integer, decimal, or scientific notation). */
    if (isdigit((unsigned char)ch) || ch == '.') {
        size_t start = p->pos;
        bool has_digit = false;
        while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos])) {
            has_digit = true;
            p->pos++;
        }
        if (p->pos < p->len && p->src[p->pos] == '.') {
            p->pos++;
            while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos])) {
                has_digit = true;
                p->pos++;
            }
        }
        if (!has_digit) {
            parser_error(p, "malformed number literal");
            return -1;
        }
        if (p->pos < p->len && (p->src[p->pos] == 'e' || p->src[p->pos] == 'E')) {
            size_t exp_pos = p->pos;
            p->pos++;
            if (p->pos < p->len && (p->src[p->pos] == '+' || p->src[p->pos] == '-')) {
                p->pos++;
            }
            size_t digits_start = p->pos;
            while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos])) {
                p->pos++;
            }
            if (p->pos == digits_start) {
                p->pos = exp_pos;
                parser_error(p, "malformed exponent in number");
                return -1;
            }
        }

        size_t num_len = p->pos - start;
        if (num_len >= 64u) {
            parser_error(p, "number literal too long");
            return -1;
        }
        char buf[64];
        memcpy(buf, p->src + start, num_len);
        buf[num_len] = '\0';
        char *endptr = NULL;
        double val = strtod(buf, &endptr);
        if (endptr == buf || !isfinite(val)) {
            parser_error(p, "number out of range");
            return -1;
        }
        RataExprNode node;
        memset(&node, 0, sizeof(node));
        node.kind = EXPR_NUM;
        node.number = val;
        node.a = -1;
        node.b = -1;
        node.c = -1;
        return push_node(p, &node);
    }

    /* Identifier: constant, variable, or function call. */
    if (isalpha((unsigned char)ch) || ch == '_') {
        size_t start = p->pos;
        while (p->pos < p->len &&
               (isalnum((unsigned char)p->src[p->pos]) || p->src[p->pos] == '_')) {
            p->pos++;
        }
        size_t id_len = p->pos - start;
        if (id_len >= 32u) {
            parser_error(p, "identifier too long");
            return -1;
        }
        char ident[32];
        memcpy(ident, p->src + start, id_len);
        ident[id_len] = '\0';

        skip_ws(p);
        if (peek_ch(p) == '(') {
            ExprFnId fn_id;
            int expected_arity = 0;
            if (!lookup_fn(ident, &fn_id, &expected_arity)) {
                parser_error(p, "unknown function name");
                return -1;
            }
            p->pos++; /* consume '(' */
            p->depth++;
            int32_t args[3] = {-1, -1, -1};
            int argc = 0;
            skip_ws(p);
            if (peek_ch(p) != ')') {
                for (;;) {
                    if (argc >= 3) {
                        parser_error(p, "too many function arguments");
                        p->depth--;
                        return -1;
                    }
                    args[argc] = parse_comparison(p);
                    if (p->has_error) {
                        p->depth--;
                        return -1;
                    }
                    argc++;
                    skip_ws(p);
                    if (peek_ch(p) == ',') {
                        p->pos++;
                    } else {
                        break;
                    }
                }
            }
            if (peek_ch(p) != ')') {
                parser_error(p, "missing ')' after function arguments");
                p->depth--;
                return -1;
            }
            p->pos++;
            p->depth--;
            if (argc != expected_arity) {
                parser_error(p, "wrong number of arguments to function");
                return -1;
            }
            RataExprNode node;
            memset(&node, 0, sizeof(node));
            node.kind = EXPR_CALL;
            node.fn_id = fn_id;
            node.a = args[0];
            node.b = args[1];
            node.c = args[2];
            return push_node(p, &node);
        }

        /* Built-in mathematical constants. */
        if (strcmp(ident, "pi") == 0 || strcmp(ident, "PI") == 0) {
            RataExprNode node;
            memset(&node, 0, sizeof(node));
            node.kind = EXPR_NUM;
            node.number = RATA_PI;
            node.a = -1;
            node.b = -1;
            node.c = -1;
            return push_node(p, &node);
        }
        if (strcmp(ident, "e") == 0) {
            RataExprNode node;
            memset(&node, 0, sizeof(node));
            node.kind = EXPR_NUM;
            node.number = RATA_E;
            node.a = -1;
            node.b = -1;
            node.c = -1;
            return push_node(p, &node);
        }

        RataExprNode node;
        memset(&node, 0, sizeof(node));
        node.kind = EXPR_VAR;
        memcpy(node.ident, ident, id_len + 1u);
        node.a = -1;
        node.b = -1;
        node.c = -1;
        return push_node(p, &node);
    }

    parser_error(p, "unexpected character in expression");
    return -1;
}

static int32_t parse_unary(ExprParser *p);

static int32_t parse_power(ExprParser *p) {
    int32_t base = parse_primary(p);
    if (p->has_error) {
        return -1;
    }
    skip_ws(p);
    if (peek_ch(p) == '^') {
        p->pos++;
        if (p->depth >= RATA_EXPR_MAX_DEPTH) {
            parser_error(p, "maximum nesting depth exceeded");
            return -1;
        }
        p->depth++;
        /* Right-associative: 2^3^2 == 2^(3^2), and 2^-3 works cleanly. */
        int32_t exp_node = parse_unary(p);
        p->depth--;
        if (p->has_error) {
            return -1;
        }
        RataExprNode node;
        memset(&node, 0, sizeof(node));
        node.kind = EXPR_POW;
        node.a = base;
        node.b = exp_node;
        node.c = -1;
        return push_node(p, &node);
    }
    return base;
}

static int32_t parse_unary(ExprParser *p) {
    skip_ws(p);
    if (p->has_error) {
        return -1;
    }
    char ch = peek_ch(p);
    if (ch == '+' || ch == '-') {
        p->pos++;
        if (p->depth >= RATA_EXPR_MAX_DEPTH) {
            parser_error(p, "maximum nesting depth exceeded");
            return -1;
        }
        p->depth++;
        int32_t operand = parse_unary(p);
        p->depth--;
        if (p->has_error) {
            return -1;
        }
        if (ch == '+') {
            return operand;
        }
        RataExprNode node;
        memset(&node, 0, sizeof(node));
        node.kind = EXPR_UNARY_NEG;
        node.a = operand;
        node.b = -1;
        node.c = -1;
        return push_node(p, &node);
    }
    return parse_power(p);
}

static int32_t parse_term(ExprParser *p) {
    int32_t lhs = parse_unary(p);
    if (p->has_error) {
        return -1;
    }
    for (;;) {
        skip_ws(p);
        char ch = peek_ch(p);
        if (ch != '*' && ch != '/') {
            break;
        }
        p->pos++;
        int32_t rhs = parse_unary(p);
        if (p->has_error) {
            return -1;
        }
        RataExprNode node;
        memset(&node, 0, sizeof(node));
        node.kind = (ch == '*') ? EXPR_MUL : EXPR_DIV;
        node.a = lhs;
        node.b = rhs;
        node.c = -1;
        lhs = push_node(p, &node);
        if (p->has_error) {
            return -1;
        }
    }
    return lhs;
}

static int32_t parse_additive(ExprParser *p) {
    int32_t lhs = parse_term(p);
    if (p->has_error) {
        return -1;
    }
    for (;;) {
        skip_ws(p);
        char ch = peek_ch(p);
        if (ch != '+' && ch != '-') {
            break;
        }
        p->pos++;
        int32_t rhs = parse_term(p);
        if (p->has_error) {
            return -1;
        }
        RataExprNode node;
        memset(&node, 0, sizeof(node));
        node.kind = (ch == '+') ? EXPR_ADD : EXPR_SUB;
        node.a = lhs;
        node.b = rhs;
        node.c = -1;
        lhs = push_node(p, &node);
        if (p->has_error) {
            return -1;
        }
    }
    return lhs;
}

static int32_t parse_comparison(ExprParser *p) {
    int32_t lhs = parse_additive(p);
    if (p->has_error) {
        return -1;
    }
    for (;;) {
        skip_ws(p);
        ExprNodeKind op;
        size_t adv = 0u;
        char c0 = peek_ch(p);
        char c1 = (p->pos + 1u < p->len) ? p->src[p->pos + 1u] : '\0';
        if (c0 == '<' && c1 == '=') {
            op = EXPR_LE;
            adv = 2u;
        } else if (c0 == '>' && c1 == '=') {
            op = EXPR_GE;
            adv = 2u;
        } else if (c0 == '=' && c1 == '=') {
            op = EXPR_EQ;
            adv = 2u;
        } else if (c0 == '!' && c1 == '=') {
            op = EXPR_NE;
            adv = 2u;
        } else if (c0 == '<') {
            op = EXPR_LT;
            adv = 1u;
        } else if (c0 == '>') {
            op = EXPR_GT;
            adv = 1u;
        } else {
            break;
        }
        p->pos += adv;
        int32_t rhs = parse_additive(p);
        if (p->has_error) {
            return -1;
        }
        RataExprNode node;
        memset(&node, 0, sizeof(node));
        node.kind = op;
        node.a = lhs;
        node.b = rhs;
        node.c = -1;
        lhs = push_node(p, &node);
        if (p->has_error) {
            return -1;
        }
    }
    return lhs;
}

RataStatus expr_parse(const char *src,
                      size_t len,
                      RataExpr *out,
                      RataExprError *err_out,
                      RataArena *arena) {
    if (src == NULL || out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    out->nodes = NULL;
    out->node_count = 0u;
    out->root = -1;
    if (err_out != NULL) {
        err_out->col = 0u;
        err_out->message[0] = '\0';
    }
    if (len == 0u) {
        if (err_out != NULL) {
            snprintf(err_out->message, sizeof(err_out->message), "empty expression");
        }
        return RATA_ERR_PARSE;
    }
    /* Reject embedded NUL bytes. */
    for (size_t i = 0u; i < len; ++i) {
        if (src[i] == '\0') {
            if (err_out != NULL) {
                err_out->col = i;
                snprintf(err_out->message, sizeof(err_out->message), "unexpected NUL byte");
            }
            return RATA_ERR_PARSE;
        }
    }

    size_t mark = rata_arena_mark(arena);
    RataExprNode *buf = (RataExprNode *)rata_arena_alloc(
        arena, RATA_EXPR_MAX_NODES * sizeof(RataExprNode), 8u);
    if (buf == NULL) {
        return RATA_ERR_OUT_OF_MEMORY;
    }

    ExprParser parser;
    memset(&parser, 0, sizeof(parser));
    parser.src = src;
    parser.len = len;
    parser.nodes = buf;
    parser.node_cap = RATA_EXPR_MAX_NODES;
    parser.err = err_out;

    int32_t root = parse_comparison(&parser);
    skip_ws(&parser);
    if (!parser.has_error && parser.pos < parser.len) {
        parser_error(&parser, "unexpected trailing characters");
    }

    if (parser.has_error || root < 0) {
        rata_arena_restore(arena, mark);
        return RATA_ERR_PARSE;
    }

    /* Compact arena allocation to actual node_count. */
    size_t used_bytes = parser.node_count * sizeof(RataExprNode);
    size_t compact_mark = mark + used_bytes;
    /* Keep exact node array pointer in arena; if buffer started after alignment pad,
     * just keep arena offset safely at current mark if compact_mark > offset. */
    if (compact_mark <= rata_arena_mark(arena)) {
        /* Note: alignment padding means actual end is ((uint8_t*)buf - arena->base) + used_bytes. */
        size_t buf_off = (size_t)((uint8_t *)buf - arena->base);
        rata_arena_restore(arena, buf_off + used_bytes);
    }

    out->nodes = buf;
    out->node_count = parser.node_count;
    out->root = root;
    return RATA_OK;
}

RataStatus expr_parse_cstr(const char *src,
                           RataExpr *out,
                           RataExprError *err_out,
                           RataArena *arena) {
    if (src == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    return expr_parse(src, strlen(src), out, err_out, arena);
}

static RataStatus eval_node(const RataExpr *expr,
                            int32_t idx,
                            const RataExprVar *vars,
                            size_t var_count,
                            size_t depth,
                            double *out) {
    if (depth > RATA_EXPR_MAX_DEPTH + 4u || idx < 0 || (size_t)idx >= expr->node_count) {
        return RATA_ERR_INVALID_ARG;
    }
    const RataExprNode *nd = &expr->nodes[idx];
    double va = 0.0;
    double vb = 0.0;
    double vc = 0.0;
    RataStatus st = RATA_OK;

    switch (nd->kind) {
        case EXPR_NUM:
            *out = nd->number;
            return RATA_OK;

        case EXPR_VAR:
            for (size_t i = 0u; i < var_count; ++i) {
                if (vars[i].name != NULL && strcmp(vars[i].name, nd->ident) == 0) {
                    *out = vars[i].value;
                    return RATA_OK;
                }
            }
            return RATA_ERR_PARSE;

        case EXPR_UNARY_NEG:
            st = eval_node(expr, nd->a, vars, var_count, depth + 1u, &va);
            if (st != RATA_OK) {
                return st;
            }
            *out = -va;
            return RATA_OK;

        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_POW:
        case EXPR_LT:
        case EXPR_LE:
        case EXPR_GT:
        case EXPR_GE:
        case EXPR_EQ:
        case EXPR_NE:
            st = eval_node(expr, nd->a, vars, var_count, depth + 1u, &va);
            if (st != RATA_OK) {
                return st;
            }
            st = eval_node(expr, nd->b, vars, var_count, depth + 1u, &vb);
            if (st != RATA_OK) {
                return st;
            }
            if (nd->kind == EXPR_ADD) {
                *out = va + vb;
            } else if (nd->kind == EXPR_SUB) {
                *out = va - vb;
            } else if (nd->kind == EXPR_MUL) {
                *out = va * vb;
            } else if (nd->kind == EXPR_DIV) {
                if (fabs(vb) == 0.0) {
                    return RATA_ERR_DIV_ZERO;
                }
                *out = va / vb;
            } else if (nd->kind == EXPR_POW) {
                if (va == 0.0 && vb < 0.0) {
                    return RATA_ERR_DIV_ZERO;
                }
                *out = pow(va, vb);
            } else if (nd->kind == EXPR_LT) {
                *out = (va < vb) ? 1.0 : 0.0;
            } else if (nd->kind == EXPR_LE) {
                *out = (va <= vb) ? 1.0 : 0.0;
            } else if (nd->kind == EXPR_GT) {
                *out = (va > vb) ? 1.0 : 0.0;
            } else if (nd->kind == EXPR_GE) {
                *out = (va >= vb) ? 1.0 : 0.0;
            } else if (nd->kind == EXPR_EQ) {
                *out = (fabs(va - vb) <= 1.0e-9) ? 1.0 : 0.0;
            } else {
                *out = (fabs(va - vb) > 1.0e-9) ? 1.0 : 0.0;
            }
            return isfinite(*out) ? RATA_OK : RATA_ERR_RANGE;

        case EXPR_CALL:
            if (nd->fn_id == FN_IF) {
                /* Short-circuit evaluation so if(x != 0, 1/x, 0) never divides by zero. */
                st = eval_node(expr, nd->a, vars, var_count, depth + 1u, &va);
                if (st != RATA_OK) {
                    return st;
                }
                int32_t branch = (va != 0.0) ? nd->b : nd->c;
                return eval_node(expr, branch, vars, var_count, depth + 1u, out);
            }
            st = eval_node(expr, nd->a, vars, var_count, depth + 1u, &va);
            if (st != RATA_OK) {
                return st;
            }
            if (nd->b >= 0) {
                st = eval_node(expr, nd->b, vars, var_count, depth + 1u, &vb);
                if (st != RATA_OK) {
                    return st;
                }
            }
            if (nd->c >= 0) {
                st = eval_node(expr, nd->c, vars, var_count, depth + 1u, &vc);
                if (st != RATA_OK) {
                    return st;
                }
            }
            switch (nd->fn_id) {
                case FN_SIN:   *out = sin(va); break;
                case FN_COS:   *out = cos(va); break;
                case FN_TAN:   *out = tan(va); break;
                case FN_ASIN:
                    if (va < -1.0 || va > 1.0) { return RATA_ERR_RANGE; }
                    *out = asin(va);
                    break;
                case FN_ACOS:
                    if (va < -1.0 || va > 1.0) { return RATA_ERR_RANGE; }
                    *out = acos(va);
                    break;
                case FN_ATAN:  *out = atan(va); break;
                case FN_ATAN2: *out = atan2(va, vb); break;
                case FN_SINH:  *out = sinh(va); break;
                case FN_COSH:  *out = cosh(va); break;
                case FN_TANH:  *out = tanh(va); break;
                case FN_EXP:   *out = exp(va); break;
                case FN_LN:
                    if (va <= 0.0) { return RATA_ERR_RANGE; }
                    *out = log(va);
                    break;
                case FN_LOG10:
                    if (va <= 0.0) { return RATA_ERR_RANGE; }
                    *out = log10(va);
                    break;
                case FN_LOG2:
                    if (va <= 0.0) { return RATA_ERR_RANGE; }
                    *out = log(va) / RATA_LN2;
                    break;
                case FN_SQRT:
                    if (va < 0.0) { return RATA_ERR_RANGE; }
                    *out = sqrt(va);
                    break;
                case FN_ABS:   *out = fabs(va); break;
                case FN_FLOOR: *out = floor(va); break;
                case FN_CEIL:  *out = ceil(va); break;
                case FN_ROUND: *out = floor(va + 0.5); break;
                case FN_MIN:   *out = (va < vb) ? va : vb; break;
                case FN_MAX:   *out = (va > vb) ? va : vb; break;
                case FN_MOD:
                    if (fabs(vb) == 0.0) { return RATA_ERR_DIV_ZERO; }
                    *out = fmod(va, vb);
                    break;
                case FN_DELTA:
                    *out = (fabs(va) <= 1.0e-9) ? 1.0 : 0.0;
                    break;
                case FN_STEP:
                    *out = (va >= -1.0e-9) ? 1.0 : 0.0;
                    break;
                case FN_RAMP:
                    *out = (va > 0.0) ? va : 0.0;
                    break;
                case FN_RECT:
                    *out = (fabs(va) <= 0.5 + 1.0e-9) ? 1.0 : 0.0;
                    break;
                case FN_TRI:
                    *out = (fabs(va) < 1.0) ? (1.0 - fabs(va)) : 0.0;
                    break;
                case FN_SINC:
                    if (fabs(va) < 1.0e-12) {
                        *out = 1.0;
                    } else {
                        double px = RATA_PI * va;
                        *out = sin(px) / px;
                    }
                    break;
                case FN_SGN:
                    if (va > 1.0e-12) {
                        *out = 1.0;
                    } else if (va < -1.0e-12) {
                        *out = -1.0;
                    } else {
                        *out = 0.0;
                    }
                    break;
                case FN_IF:
                    *out = (va != 0.0) ? vb : vc;
                    break;
                default:
                    return RATA_ERR_INVALID_ARG;
            }
            return isfinite(*out) ? RATA_OK : RATA_ERR_RANGE;

        default:
            return RATA_ERR_INVALID_ARG;
    }
}

RataStatus expr_eval(const RataExpr *expr,
                     const RataExprVar *vars,
                     size_t var_count,
                     double *result_out) {
    if (expr == NULL || expr->nodes == NULL || result_out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (var_count > 0u && vars == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    double val = 0.0;
    RataStatus st = eval_node(expr, expr->root, vars, var_count, 0u, &val);
    if (st != RATA_OK) {
        *result_out = 0.0;
        return st;
    }
    *result_out = val;
    return RATA_OK;
}
