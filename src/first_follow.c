/*
 * Module 3a - FIRST / FOLLOW (Owner: Member 3 - Nitin, 24BCE2819)
 */
#include "first_follow.h"

#include <string.h>

/* FIRST of a string X1 X2 ... Xn; *nullable set if the whole string can vanish */
SymSet first_of_seq(const Sets *s, const int *syms, int n, int *nullable)
{
    SymSet r = 0;
    int i;
    for (i = 0; i < n; i++) {
        r |= s->first[syms[i]];
        if (!s->nullable[syms[i]]) {
            *nullable = 0;
            return r;
        }
    }
    *nullable = 1;
    return r;
}

void compute_sets(const Grammar *g, Sets *s)
{
    int i, k, changed, nul;

    memset(s, 0, sizeof *s);
    for (i = 0; i < g->nterms; i++) s->first[g->terms[i]] = SYMBIT(g->terms[i]);

    /* FIRST */
    do {
        changed = 0;
        for (i = 0; i < g->nprods; i++) {
            const Production *p = &g->prods[i];
            SymSet f = first_of_seq(s, p->rhs, p->len, &nul);
            if ((s->first[p->head] | f) != s->first[p->head]) {
                s->first[p->head] |= f;
                changed = 1;
            }
            if (nul && !s->nullable[p->head]) {
                s->nullable[p->head] = 1;
                changed = 1;
            }
        }
    } while (changed);

    /* FOLLOW */
    s->follow[g->aug_start] = SYMBIT(g->end);
    do {
        changed = 0;
        for (i = 0; i < g->nprods; i++) {
            const Production *p = &g->prods[i];
            for (k = 0; k < p->len; k++) {
                int sym = p->rhs[k];
                SymSet add;
                if (g->is_terminal[sym]) continue;
                add = first_of_seq(s, p->rhs + k + 1, p->len - k - 1, &nul);
                if (nul) add |= s->follow[p->head];
                if ((s->follow[sym] | add) != s->follow[sym]) {
                    s->follow[sym] |= add;
                    changed = 1;
                }
            }
        }
    } while (changed);
}

static void fmt_set(const Grammar *g, SymSet set, int with_eps, char *buf, size_t n)
{
    int i, first = 1;
    snprintf(buf, n, "{ ");
    for (i = 0; i < g->nterms; i++) {
        if (!(set & SYMBIT(g->terms[i]))) continue;
        str_append(buf, n, "%s%s", first ? "" : ", ", g->names[g->terms[i]]);
        first = 0;
    }
    if (with_eps) str_append(buf, n, "%s%s", first ? "" : ", ", EPSILON_TEXT);
    str_append(buf, n, " }");
}

void sets_print(const Grammar *g, const Sets *s, FILE *out)
{
    char f1[512], f2[512];
    int i;
    fprintf(out, "%-12s %-28s %s\n", "Nonterminal", "FIRST", "FOLLOW");
    for (i = 1; i < g->nnonterms; i++) {
        int nt = g->nonterms[i];
        fmt_set(g, s->first[nt], s->nullable[nt], f1, sizeof f1);
        fmt_set(g, s->follow[nt], 0, f2, sizeof f2);
        fprintf(out, "%-12s %-28s %s\n", g->names[nt], f1, f2);
    }
}
