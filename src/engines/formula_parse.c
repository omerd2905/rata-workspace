#include "engines/formula_parse.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

typedef struct FormulaParser {
    const char *src;
    size_t len;
    size_t pos;
    size_t depth;
    FormulaNode *nodes;
    size_t node_count;
    size_t node_cap;
    bool err;
} FormulaParser;

static int32_t alloc_node(FormulaParser *p, FormulaNodeKind kind) {
    if (p->node_count >= p->node_cap) {
        p->err = true;
        return -1;
    }
    int32_t id = (int32_t)p->node_count++;
    FormulaNode *n = &p->nodes[id];
    memset(n, 0, sizeof(*n));
    n->kind = kind;
    n->first_child = -1;
    n->next_sibling = -1;
    n->child_a = -1;
    n->child_b = -1;
    n->child_c = -1;
    return id;
}

static bool lookup_symbol(const char *name, FormulaSymKind *out) {
    typedef struct SymMap {
        const char *name;
        FormulaSymKind sym;
    } SymMap;
    static const SymMap map[] = {
        {"alpha", FSYM_ALPHA},       {"beta", FSYM_BETA},
        {"gamma", FSYM_GAMMA},       {"delta", FSYM_DELTA_LOWER},
        {"Delta", FSYM_DELTA_UPPER}, {"epsilon", FSYM_EPSILON},
        {"zeta", FSYM_ZETA},         {"eta", FSYM_ETA},
        {"theta", FSYM_THETA},       {"lambda", FSYM_LAMBDA},
        {"mu", FSYM_MU},             {"pi", FSYM_PI},
        {"rho", FSYM_RHO},           {"sigma", FSYM_SIGMA_LOWER},
        {"Sigma", FSYM_SIGMA_UPPER}, {"tau", FSYM_TAU},
        {"phi", FSYM_PHI_LOWER},     {"Phi", FSYM_PHI_UPPER},
        {"omega", FSYM_OMEGA_LOWER}, {"Omega", FSYM_OMEGA_UPPER},
        {"pm", FSYM_PM},             {"times", FSYM_TIMES},
        {"cdot", FSYM_CDOT},         {"le", FSYM_LE},
        {"leq", FSYM_LE},            {"ge", FSYM_GE},
        {"geq", FSYM_GE},            {"ne", FSYM_NE},
        {"neq", FSYM_NE},            {"approx", FSYM_APPROX},
        {"infty", FSYM_INFTY},       {"inf", FSYM_INFTY},
        {"sum", FSYM_SUM},           {"int", FSYM_INT},
        {"to", FSYM_ARROW},          {"rightarrow", FSYM_ARROW},
        {"angle", FSYM_ANGLE},       {"deg", FSYM_DEG}
    };
    size_t count = sizeof(map) / sizeof(map[0]);
    for (size_t i = 0u; i < count; ++i) {
        if (strcmp(name, map[i].name) == 0) {
            *out = map[i].sym;
            return true;
        }
    }
    return false;
}

const char *formula_sym_ascii(FormulaSymKind sym) {
    switch (sym) {
        case FSYM_ALPHA:       return "a";
        case FSYM_BETA:        return "b";
        case FSYM_GAMMA:       return "g";
        case FSYM_DELTA_LOWER: return "d";
        case FSYM_DELTA_UPPER: return "D";
        case FSYM_EPSILON:     return "e";
        case FSYM_ZETA:        return "z";
        case FSYM_ETA:         return "n";
        case FSYM_THETA:       return "th";
        case FSYM_LAMBDA:      return "lam";
        case FSYM_MU:          return "u";
        case FSYM_PI:          return "pi";
        case FSYM_RHO:         return "rho";
        case FSYM_SIGMA_LOWER: return "sig";
        case FSYM_SIGMA_UPPER: return "SUM";
        case FSYM_TAU:         return "tau";
        case FSYM_PHI_LOWER:   return "phi";
        case FSYM_PHI_UPPER:   return "Phi";
        case FSYM_OMEGA_LOWER: return "w";
        case FSYM_OMEGA_UPPER: return "Ohm";
        case FSYM_PM:          return "+/-";
        case FSYM_TIMES:       return "x";
        case FSYM_CDOT:        return "*";
        case FSYM_LE:          return "<=";
        case FSYM_GE:          return ">=";
        case FSYM_NE:          return "!=";
        case FSYM_APPROX:      return "~=";
        case FSYM_INFTY:       return "inf";
        case FSYM_SUM:         return "SUM";
        case FSYM_INT:         return "INT";
        case FSYM_ARROW:       return "->";
        case FSYM_ANGLE:       return "<";
        case FSYM_DEG:         return "deg";
        default:               return "?";
    }
}

static int32_t parse_row(FormulaParser *p, bool stop_on_rbrace);

static int32_t parse_braced_group(FormulaParser *p) {
    if (p->pos >= p->len || p->src[p->pos] != '{') {
        p->err = true;
        return -1;
    }
    p->pos++; /* consume '{' */
    if (p->depth >= RATA_FORMULA_MAX_DEPTH) {
        p->err = true;
        return -1;
    }
    p->depth++;
    int32_t row = parse_row(p, true);
    p->depth--;
    if (p->err) {
        return -1;
    }
    if (p->pos >= p->len || p->src[p->pos] != '}') {
        p->err = true;
        return -1;
    }
    p->pos++; /* consume '}' */
    return row;
}

static int32_t parse_atom(FormulaParser *p);

static int32_t parse_script_arg(FormulaParser *p) {
    while (p->pos < p->len && p->src[p->pos] == ' ') {
        p->pos++;
    }
    if (p->pos >= p->len) {
        p->err = true;
        return -1;
    }
    if (p->src[p->pos] == '{') {
        return parse_braced_group(p);
    }
    /* Single symbol or single ASCII character for shorthand like x^2 or a_0. */
    if (p->src[p->pos] == '\\') {
        return parse_atom(p);
    }
    char c = p->src[p->pos];
    if (c == '}' || c == '^' || c == '_') {
        p->err = true;
        return -1;
    }
    p->pos++;
    int32_t id = alloc_node(p, FNODE_TEXT);
    if (id < 0) {
        return -1;
    }
    p->nodes[id].text[0] = c;
    p->nodes[id].text[1] = '\0';
    return id;
}

static int32_t parse_command(FormulaParser *p, const char *cmd) {
    if (strcmp(cmd, "frac") == 0) {
        int32_t num = parse_braced_group(p);
        if (p->err) {
            return -1;
        }
        int32_t den = parse_braced_group(p);
        if (p->err) {
            return -1;
        }
        int32_t id = alloc_node(p, FNODE_FRAC);
        if (id < 0) {
            return -1;
        }
        p->nodes[id].child_a = num;
        p->nodes[id].child_b = den;
        return id;
    }
    if (strcmp(cmd, "sqrt") == 0) {
        int32_t rad = parse_braced_group(p);
        if (p->err) {
            return -1;
        }
        int32_t id = alloc_node(p, FNODE_SQRT);
        if (id < 0) {
            return -1;
        }
        p->nodes[id].child_a = rad;
        return id;
    }
    FormulaSymKind sym;
    if (lookup_symbol(cmd, &sym)) {
        int32_t id = alloc_node(p, FNODE_SYMBOL);
        if (id < 0) {
            return -1;
        }
        p->nodes[id].sym = sym;
        return id;
    }
    p->err = true;
    return -1;
}

static int32_t parse_atom(FormulaParser *p) {
    if (p->pos >= p->len || p->err) {
        return -1;
    }
    char c = p->src[p->pos];
    if (c == '{') {
        return parse_braced_group(p);
    }
    if (c == '\\') {
        p->pos++;
        size_t start = p->pos;
        while (p->pos < p->len && isalpha((unsigned char)p->src[p->pos])) {
            p->pos++;
        }
        size_t clen = p->pos - start;
        if (clen == 0u || clen >= 24u) {
            p->err = true;
            return -1;
        }
        char cmd[24];
        memcpy(cmd, p->src + start, clen);
        cmd[clen] = '\0';
        return parse_command(p, cmd);
    }

    /* Bare keywords frac{...}{...} and sqrt{...} allowed without backslash too. */
    if (p->pos + 5u <= p->len && strncmp(p->src + p->pos, "frac{", 5u) == 0) {
        p->pos += 4u;
        return parse_command(p, "frac");
    }
    if (p->pos + 5u <= p->len && strncmp(p->src + p->pos, "sqrt{", 5u) == 0) {
        p->pos += 4u;
        return parse_command(p, "sqrt");
    }

    /* Reject stray control/script chars as standalone atoms. */
    if (c == '}' || c == '^' || c == '_') {
        p->err = true;
        return -1;
    }

    /* Collect plain text run, stopping before special tokens or right before a scripted char. */
    int32_t id = alloc_node(p, FNODE_TEXT);
    if (id < 0) {
        return -1;
    }
    size_t out_len = 0u;
    while (p->pos < p->len && out_len + 1u < RATA_FORMULA_TEXT_MAX) {
        char ch = p->src[p->pos];
        if (ch == '{' || ch == '}' || ch == '\\' || ch == '^' || ch == '_') {
            break;
        }
        if (p->pos + 5u <= p->len &&
            (strncmp(p->src + p->pos, "frac{", 5u) == 0 ||
             strncmp(p->src + p->pos, "sqrt{", 5u) == 0)) {
            break;
        }
        /* If next char is '^' or '_', split out the single base character so only it gets scripted. */
        if (out_len > 0u && p->pos + 1u < p->len &&
            (p->src[p->pos + 1u] == '^' || p->src[p->pos + 1u] == '_')) {
            break;
        }
        unsigned char uch = (unsigned char)ch;
        if (uch < 0x20u || uch >= 0x7Fu) {
            p->err = true;
            return -1;
        }
        p->nodes[id].text[out_len++] = ch;
        p->pos++;
        /* If immediately followed by '^' or '_', stop so this single char is the script base. */
        if (p->pos < p->len && (p->src[p->pos] == '^' || p->src[p->pos] == '_')) {
            break;
        }
    }
    p->nodes[id].text[out_len] = '\0';
    return id;
}

static int32_t parse_row(FormulaParser *p, bool stop_on_rbrace) {
    int32_t row_id = alloc_node(p, FNODE_ROW);
    if (row_id < 0) {
        return -1;
    }
    int32_t tail = -1;

    while (p->pos < p->len && !p->err) {
        char c = p->src[p->pos];
        if (c == '}') {
            if (stop_on_rbrace) {
                break;
            }
            p->err = true;
            return -1;
        }

        int32_t item = parse_atom(p);
        if (p->err || item < 0) {
            return -1;
        }

        /* Attach optional subscript and/or superscript in either order. */
        int32_t sub = -1;
        int32_t sup = -1;
        while (p->pos < p->len && (p->src[p->pos] == '_' || p->src[p->pos] == '^')) {
            char sc = p->src[p->pos++];
            if (sc == '_') {
                if (sub >= 0) {
                    p->err = true;
                    return -1;
                }
                sub = parse_script_arg(p);
            } else {
                if (sup >= 0) {
                    p->err = true;
                    return -1;
                }
                sup = parse_script_arg(p);
            }
            if (p->err) {
                return -1;
            }
        }

        if (sub >= 0 || sup >= 0) {
            int32_t wrap = alloc_node(p, FNODE_SCRIPT);
            if (wrap < 0) {
                return -1;
            }
            p->nodes[wrap].child_a = item;
            p->nodes[wrap].child_b = sub;
            p->nodes[wrap].child_c = sup;
            item = wrap;
        }

        if (tail < 0) {
            p->nodes[row_id].first_child = item;
        } else {
            p->nodes[tail].next_sibling = item;
        }
        tail = item;
    }

    return row_id;
}

RataStatus formula_parse(const char *src, size_t len, FormulaTree *out, RataArena *arena) {
    if (src == NULL || out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    out->nodes = NULL;
    out->node_count = 0u;
    out->root = -1;
    if (len > 4096u) {
        return RATA_ERR_PARSE;
    }
    for (size_t i = 0u; i < len; ++i) {
        if (src[i] == '\0') {
            return RATA_ERR_PARSE;
        }
    }

    size_t mark = rata_arena_mark(arena);
    FormulaNode *buf = (FormulaNode *)rata_arena_alloc(
        arena, RATA_FORMULA_MAX_NODES * sizeof(FormulaNode), 8u);
    if (buf == NULL) {
        return RATA_ERR_OUT_OF_MEMORY;
    }

    FormulaParser p;
    memset(&p, 0, sizeof(p));
    p.src = src;
    p.len = len;
    p.nodes = buf;
    p.node_cap = RATA_FORMULA_MAX_NODES;

    int32_t root = parse_row(&p, false);
    if (p.err || root < 0 || p.pos != p.len) {
        rata_arena_restore(arena, mark);
        return RATA_ERR_PARSE;
    }

    size_t buf_off = (size_t)((uint8_t *)buf - arena->base);
    rata_arena_restore(arena, buf_off + p.node_count * sizeof(FormulaNode));

    out->nodes = buf;
    out->node_count = p.node_count;
    out->root = root;
    return RATA_OK;
}

RataStatus formula_parse_cstr(const char *src, FormulaTree *out, RataArena *arena) {
    if (src == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    return formula_parse(src, strlen(src), out, arena);
}
