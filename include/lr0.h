/*
 * Module 2 - LR(0) Items, closure(), GOTO() and the Canonical Collection
 * (Owner: Member 2)
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

int  lr0_build(LR0 *a, const Grammar *g, char *err, size_t errlen);   /* 0 / -1 */

int  lr0_next_symbol(const Grammar *g, Item it);                       /* -1 if dot at end */
int  lr0_closure(const Grammar *g, ItemSet *set);                       /* in place; -1 on overflow */
int  lr0_goto(const Grammar *g, const ItemSet *from, int sym, ItemSet *out);

void lr0_item_str(const Grammar *g, Item it, char *buf, size_t n);
void lr0_print(const LR0 *a, FILE *out);

#endif
