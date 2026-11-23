#ifndef RATA_ENGINES_EXPR_H
#define RATA_ENGINES_EXPR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/arena.h"
#include "core/status.h"

#define RATA_EXPR_MAX_DEPTH  64u
#define RATA_EXPR_MAX_NODES  1024u
#define RATA_EXPR_ERR_BUFSZ  128u

/* Variable binding passed at evaluation time. */
typedef struct RataExprVar {
    const char *name;
    double value;
} RataExprVar;

typedef struct RataExprError {
    size_t col;
    char message[RATA_EXPR_ERR_BUFSZ];
} RataExprError;

/* Opaque AST node array stored inside caller-provided RataArena. */
typedef struct RataExprNode RataExprNode;

/* Compiled expression handle.
 * `nodes` memory is owned by the RataArena passed to expr_parse(). */
typedef struct RataExpr {
    RataExprNode *nodes;
    size_t node_count;
    int32_t root;
} RataExpr;

/* Parses expression text (length `len`, or NUL-terminated via expr_parse_cstr).
 * Allocates AST nodes inside `arena` (owned by `arena`).
 * On error, writes diagnostic details to `err_out` (if non-NULL) and returns non-OK. */
RataStatus expr_parse(const char *src,
                      size_t len,
                      RataExpr *out,
                      RataExprError *err_out,
                      RataArena *arena);

RataStatus expr_parse_cstr(const char *src,
                           RataExpr *out,
                           RataExprError *err_out,
                           RataArena *arena);

/* Evaluates parsed expression with caller-supplied variable bindings.
 * Returns RATA_ERR_DIV_ZERO on division by zero, RATA_ERR_RANGE on domain error/NaN/Inf,
 * or RATA_ERR_PARSE if an unknown variable is referenced. */
RataStatus expr_eval(const RataExpr *expr,
                     const RataExprVar *vars,
                     size_t var_count,
                     double *result_out);

#endif /* RATA_ENGINES_EXPR_H */
