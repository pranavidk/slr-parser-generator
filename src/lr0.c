/*
 * Module 2 - LR(0) Items, closure(), GOTO(), Canonical Collection (Owner: Member 2 - Prithvi, 24BCE2778)
 */
#include "lr0.h"

#include <stdlib.h>
#include <string.h>

int lr0_next_symbol(const Grammar *g, Item it)
{
    const Production *p = &g->prods[it.prod];
    return it.dot < p->len ? p->rhs[it.dot] : -1;
}

static int cmp_item(const void *a, const void *b)
{
    const Item *x = a, *y = b;
    if (x->prod != y->prod) return x->prod - y->prod;
    return x->dot - y->dot;
}

/* If A -> a.Bb is in I, add B -> .g for every production B -> g.
 * Repeat until nothing changes (the array doubles as the worklist). */
int lr0_closure(const Grammar *g, ItemSet *set)
{
    static char in[MAX_PRODS][MAX_RHS + 1];
    int i, p;

    memset(in, 0, sizeof in);
    for (i = 0; i < set->n; i++) in[set->items[i].prod][set->items[i].dot] = 1;

    for (i = 0; i < set->n; i++) {
        int sym = lr0_next_symbol(g, set->items[i]);
        if (sym < 0 || g->is_terminal[sym]) continue;
        for (p = 0; p < g->nprods; p++) {
            if (g->prods[p].head != sym || in[p][0]) continue;
            if (set->n >= MAX_ITEMS) return -1;
            in[p][0] = 1;
            set->items[set->n].prod = p;
            set->items[set->n].dot = 0;
            set->n++;
        }
    }
    qsort(set->items, (size_t)set->n, sizeof(Item), cmp_item);
    return 0;
}

/* Advance the dot over `sym` in every item where it is next, then close. */
int lr0_goto(const Grammar *g, const ItemSet *from, int sym, ItemSet *out)
{
    int i;
    out->n = 0;
    for (i = 0; i < from->n; i++) {
        if (lr0_next_symbol(g, from->items[i]) == sym) {
            out->items[out->n] = from->items[i];
            out->items[out->n].dot++;
            out->n++;
        }
    }
    if (out->n == 0) return 0;
    return lr0_closure(g, out);
}

static int same_set(const ItemSet *a, const ItemSet *b)
{
    return a->n == b->n && memcmp(a->items, b->items, (size_t)a->n * sizeof(Item)) == 0;
}

int lr0_build(LR0 *a, const Grammar *g, char *err, size_t errlen)
{
    ItemSet tmp;
    int i, k, j;

    a->g = g;
    memset(a->trans, -1, sizeof a->trans);
    a->nstates = 1;
    a->states[0].n = 1;
    a->states[0].items[0].prod = 0;
    a->states[0].items[0].dot = 0;
    if (lr0_closure(g, &a->states[0]) < 0) goto overflow;

    for (i = 0; i < a->nstates; i++) {
        int seen[MAX_SYMBOLS] = {0};
        /* Symbols in the order they appear after a dot -> stable numbering */
        for (k = 0; k < a->states[i].n; k++) {
            int sym = lr0_next_symbol(g, a->states[i].items[k]);
            if (sym < 0 || seen[sym]) continue;
            seen[sym] = 1;
            if (lr0_goto(g, &a->states[i], sym, &tmp) < 0) goto overflow;
            for (j = 0; j < a->nstates; j++)
                if (same_set(&a->states[j], &tmp)) break;
            if (j == a->nstates) {
                if (a->nstates >= MAX_STATES) {
                    set_error(err, errlen, "Too many LR(0) states (max %d).", MAX_STATES);
                    return -1;
                }
                a->states[a->nstates++] = tmp;
            }
            a->trans[i][sym] = j;
        }
    }
    return 0;

overflow:
    set_error(err, errlen, "An LR(0) state has more than %d items.", MAX_ITEMS);
    return -1;
}

void lr0_item_str(const Grammar *g, Item it, char *buf, size_t n)
{
    const Production *p = &g->prods[it.prod];
    int k;
    snprintf(buf, n, "%s ->", g->names[p->head]);
    for (k = 0; k <= p->len; k++) {
        if (k == it.dot) str_append(buf, n, " .");
        if (k < p->len) str_append(buf, n, " %s", g->names[p->rhs[k]]);
    }
}

void lr0_print(const LR0 *a, FILE *out)
{
    const Grammar *g = a->g;
    char buf[512];
    int i, k;

    for (i = 0; i < a->nstates; i++) {
        int seen[MAX_SYMBOLS] = {0}, first = 1;
        fprintf(out, "I%d:\n", i);
        for (k = 0; k < a->states[i].n; k++) {
            lr0_item_str(g, a->states[i].items[k], buf, sizeof buf);
            fprintf(out, "    %s\n", buf);
        }
        for (k = 0; k < a->states[i].n; k++) {
            int sym = lr0_next_symbol(g, a->states[i].items[k]);
            if (sym < 0 || seen[sym]) continue;
            seen[sym] = 1;
            fprintf(out, "%s--%s--> I%d", first ? "    " : "   ", g->names[sym], a->trans[i][sym]);
            first = 0;
        }
        fprintf(out, first ? "\n" : "\n\n");
    }
}
