/*
 * Module 2 - LR(0) Items, closure(), GOTO() and the Canonical Collection
 * (Owner: Member 2 - Prithvi, 24BCE2778)
 *
 * An LR(0) item is (production index, dot position). A state is a sorted
 * set of items. The canonical collection starts from
 * I0 = closure({S' -> .S}) and applies GOTO on every grammar symbol until
 * no new state appears (fixed point).
 */
#ifndef LR0_H
#define LR0_H

#include "grammar.h"

#define MAX_STATES 256
#define MAX_ITEMS  256

typedef struct {
    int prod;
    int dot;
} Item;

typedef struct {
    Item items[MAX_ITEMS];   /* kept sorted by (prod, dot) */
    int  n;
} ItemSet;

typedef struct {
    const Grammar *g;
    ItemSet states[MAX_STATES];
    int nstates;
    int trans[MAX_STATES][MAX_SYMBOLS];   /* GOTO(state, symbol) or -1 */
} LR0;

/* Build the canonical LR(0) collection of g into *a, filling states[] and
 * trans[]. Returns 0, or -1 with a message in err if MAX_STATES or
 * MAX_ITEMS is exceeded. g must outlive a. */
int  lr0_build(LR0 *a, const Grammar *g, char *err, size_t errlen);   /* 0 / -1 */

int  lr0_next_symbol(const Grammar *g, Item it);                       /* -1 if dot at end */

/* Close `set` in place and sort it. Returns -1 if the closure would exceed
 * MAX_ITEMS. Uses a static scratch table, so it is not reentrant. */
int  lr0_closure(const Grammar *g, ItemSet *set);                       /* in place; -1 on overflow */

/* out = closure of the items of `from` with the dot moved over `sym`.
 * out->n == 0 means there is no transition on sym. Returns 0, or -1 on
 * overflow. */
int  lr0_goto(const Grammar *g, const ItemSet *from, int sym, ItemSet *out);

/* Write an item as text with the dot, e.g. "E -> E . + T". */
void lr0_item_str(const Grammar *g, Item it, char *buf, size_t n);

/* Print every state I0..In with its items and its GOTO transitions. */
void lr0_print(const LR0 *a, FILE *out);

#endif
