#ifndef RATA_ENGINES_FORMULA_PARSE_H
#define RATA_ENGINES_FORMULA_PARSE_H

#include <stddef.h>
#include <stdint.h>

#include "core/arena.h"
#include "core/status.h"

#define RATA_FORMULA_MAX_DEPTH  16u
#define RATA_FORMULA_MAX_NODES  512u
#define RATA_FORMULA_TEXT_MAX   32u

typedef enum FormulaSymKind {
    FSYM_NONE = 0,
    FSYM_ALPHA,
    FSYM_BETA,
    FSYM_GAMMA,
    FSYM_DELTA_LOWER,
    FSYM_DELTA_UPPER,
    FSYM_EPSILON,
    FSYM_ZETA,
    FSYM_ETA,
    FSYM_THETA,
    FSYM_LAMBDA,
    FSYM_MU,
    FSYM_PI,
    FSYM_RHO,
    FSYM_SIGMA_LOWER,
    FSYM_SIGMA_UPPER,
    FSYM_TAU,
    FSYM_PHI_LOWER,
    FSYM_PHI_UPPER,
    FSYM_OMEGA_LOWER,
    FSYM_OMEGA_UPPER,
    FSYM_PM,
    FSYM_TIMES,
    FSYM_CDOT,
    FSYM_LE,
    FSYM_GE,
    FSYM_NE,
    FSYM_APPROX,
    FSYM_INFTY,
    FSYM_SUM,
    FSYM_INT,
    FSYM_ARROW,
    FSYM_ANGLE,
    FSYM_DEG
} FormulaSymKind;

typedef enum FormulaNodeKind {
    FNODE_ROW = 0,     /* Horizontal sequence: linked via first_child -> next_sibling */
    FNODE_TEXT = 1,    /* Plain ASCII run in `text` */
    FNODE_SYMBOL = 2,  /* Greek letter or math operator in `sym` */
    FNODE_FRAC = 3,    /* child_a = numerator, child_b = denominator */
    FNODE_SQRT = 4,    /* child_a = radicand */
    FNODE_SCRIPT = 5   /* child_a = base, child_b = sub (-1 if none), child_c = sup (-1 if none) */
} FormulaNodeKind;

typedef struct FormulaNode {
    FormulaNodeKind kind;
    FormulaSymKind sym;
    char text[RATA_FORMULA_TEXT_MAX];
    int32_t first_child;
    int32_t next_sibling;
    int32_t child_a;
    int32_t child_b;
    int32_t child_c;
} FormulaNode;

/* Parsed formula AST. `nodes` memory is owned by the RataArena passed to formula_parse(). */
typedef struct FormulaTree {
    FormulaNode *nodes;
    size_t node_count;
    int32_t root;
} FormulaTree;

/* Parses minimal EE formula markup into a validated AST in `arena` (owned by `arena`).
 * Supported syntax:
 *   frac{num}{den}, \frac{num}{den}, sqrt{x}, \sqrt{x},
 *   base^{sup}, base_{sub}, base_a^b, {group},
 *   \alpha .. \Omega, \pm, \times, \cdot, \le, \ge, \ne, \approx, \infty, \sum, \int, \to, \angle, \deg.
 * Rejects malformed input safely without crashing or overflowing stack. */
RataStatus formula_parse(const char *src, size_t len, FormulaTree *out, RataArena *arena);
RataStatus formula_parse_cstr(const char *src, FormulaTree *out, RataArena *arena);

/* Returns short ASCII fallback text for a FormulaSymKind (e.g. "omega", "+/-"). */
const char *formula_sym_ascii(FormulaSymKind sym);

#endif /* RATA_ENGINES_FORMULA_PARSE_H */
