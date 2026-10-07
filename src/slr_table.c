/*
 * Module 3b - SLR ACTION/GOTO table + conflict detection (Owner: Member 3 - Nitin, 24BCE2819)
 */
#include "slr_table.h"

#include <string.h>

static int same_action(Action a, Action b) { return a.kind == b.kind && a.target == b.target; }

static void set_action(SLRTable *t, int state, int term, Action a)
{
    Action *cell = &t->action[state][term];
    Conflict *c = NULL;
    int i, best_reduce = -1;

    if (cell->kind == ACT_NONE) {
        *cell = a;
        return;
    }
    if (same_action(*cell, a)) return;

    /* Conflict: one record per cell, extended if more actions arrive */
    for (i = 0; i < t->nconflicts; i++)
        if (t->conflicts[i].state == state && t->conflicts[i].term == term) c = &t->conflicts[i];
    if (!c) {
        if (t->nconflicts >= MAX_CONFLICTS) return;
        c = &t->conflicts[t->nconflicts++];
        memset(c, 0, sizeof *c);
        c->state = state;
        c->term = term;
        c->acts[c->nacts++] = *cell;
    }
    for (i = 0; i < c->nacts; i++)
        if (same_action(c->acts[i], a)) break;
    if (i == c->nacts && c->nacts < MAX_CONFLICT_ACTS) c->acts[c->nacts++] = a;

    /* Resolve: shift > accept > lowest-numbered reduce */
    c->shift_reduce = 0;
    c->chosen.kind = ACT_NONE;
    for (i = 0; i < c->nacts; i++) {
        if (c->acts[i].kind == ACT_SHIFT) {
            c->shift_reduce = 1;
            c->chosen = c->acts[i];
        } else if (c->acts[i].kind == ACT_REDUCE &&
                   (best_reduce < 0 || c->acts[i].target < c->acts[best_reduce].target)) {
            best_reduce = i;
        }
    }
    if (c->chosen.kind == ACT_NONE)
        for (i = 0; i < c->nacts; i++)
            if (c->acts[i].kind == ACT_ACCEPT) c->chosen = c->acts[i];
    if (c->chosen.kind == ACT_NONE && best_reduce >= 0) c->chosen = c->acts[best_reduce];
    *cell = c->chosen;
}

void table_build(SLRTable *t, const Grammar *g, const LR0 *lr, const Sets *sets)
{
    int i, k, j;

    memset(t->action, 0, sizeof t->action);
    memset(t->go, -1, sizeof t->go);
    t->g = g;
    t->lr = lr;
    t->sets = sets;
    t->nconflicts = 0;

    for (i = 0; i < lr->nstates; i++) {
        const ItemSet *st = &lr->states[i];
        for (k = 0; k < st->n; k++) {
            Item it = st->items[k];
            const Production *p = &g->prods[it.prod];
            int sym = lr0_next_symbol(g, it);
            Action a;

            if (sym >= 0) {                                  /* Shift */
                if (g->is_terminal[sym]) {
                    a.kind = ACT_SHIFT;
                    a.target = lr->trans[i][sym];
                    set_action(t, i, sym, a);
                }
            } else if (p->head == g->aug_start) {            /* Accept */
                a.kind = ACT_ACCEPT;
                a.target = -1;
                set_action(t, i, g->end, a);
            } else {                                         /* Reduce on FOLLOW(A) */
                a.kind = ACT_REDUCE;
                a.target = it.prod;
                for (j = 0; j < g->nterms; j++)
                    if (sets->follow[p->head] & SYMBIT(g->terms[j]))
                        set_action(t, i, g->terms[j], a);
            }
        }
        for (j = 1; j < g->nnonterms; j++) {                 /* GOTO */
            int nt = g->nonterms[j];
            t->go[i][nt] = lr->trans[i][nt];
        }
    }
}

int table_is_slr(const SLRTable *t) { return t->nconflicts == 0; }

int table_expected(const SLRTable *t, int state, int *out)
{
    int i, n = 0;
    for (i = 0; i < t->g->nterms; i++)
        if (t->action[state][t->g->terms[i]].kind != ACT_NONE) out[n++] = t->g->terms[i];
    return n;
}

void action_str(Action a, char *buf, size_t n)
{
    switch (a.kind) {
    case ACT_SHIFT:  snprintf(buf, n, "s%d", a.target); break;
    case ACT_REDUCE: snprintf(buf, n, "r%d", a.target); break;
    case ACT_ACCEPT: snprintf(buf, n, "acc"); break;
    default:         buf[0] = '\0'; break;
    }
}

static void centered(FILE *out, const char *s, int w)
{
    int len = (int)strlen(s), left = (w - len) / 2;
    if (left < 0) left = 0;
    fprintf(out, "%*s%s%*s", left, "", s, w - len - left > 0 ? w - len - left : 0, "");
}

void table_print(const SLRTable *t, FILE *out)
{
    const Grammar *g = t->g;
    const int w = 6;
    char buf[32];
    int i, j, total;

    fprintf(out, "%-6s|", "");
    centered(out, "ACTION", w * g->nterms);
    fprintf(out, "|");
    centered(out, "GOTO", w * (g->nnonterms - 1));
    fprintf(out, "\n%-6s|", "State");
    for (j = 0; j < g->nterms; j++) centered(out, g->names[g->terms[j]], w);
    fprintf(out, "|");
    for (j = 1; j < g->nnonterms; j++) centered(out, g->names[g->nonterms[j]], w);
    fprintf(out, "\n");
    total = 8 + w * (g->nterms + g->nnonterms - 1);
    for (j = 0; j < total; j++) fputc('-', out);
    fprintf(out, "\n");

    for (i = 0; i < t->lr->nstates; i++) {
        fprintf(out, "%-6d|", i);
        for (j = 0; j < g->nterms; j++) {
            action_str(t->action[i][g->terms[j]], buf, sizeof buf);
            centered(out, buf, w);
        }
        fprintf(out, "|");
        for (j = 1; j < g->nnonterms; j++) {
            int gt = t->go[i][g->nonterms[j]];
            if (gt >= 0) snprintf(buf, sizeof buf, "%d", gt);
            else buf[0] = '\0';
            centered(out, buf, w);
        }
        fprintf(out, "\n");
    }
}

/* Is this item responsible for an action on `term`? */
static int item_involved(const SLRTable *t, Item it, int term)
{
    const Grammar *g = t->g;
    int sym = lr0_next_symbol(g, it);
    int head = g->prods[it.prod].head;
    if (sym >= 0) return sym == term;
    return head != g->aug_start && (t->sets->follow[head] & SYMBIT(term));
}

void table_print_conflicts(const SLRTable *t, FILE *out)
{
    const Grammar *g = t->g;
    char buf[256];
    int i, k;

    if (t->nconflicts == 0) {
        fprintf(out, "No conflicts - grammar is SLR(1).\n");
        return;
    }
    fprintf(out, "%d conflict(s) - grammar is NOT SLR(1):\n", t->nconflicts);
    for (i = 0; i < t->nconflicts; i++) {
        const Conflict *c = &t->conflicts[i];
        const ItemSet *st = &t->lr->states[c->state];
        int first = 1;
        fprintf(out, "  - %s conflict in state I%d on '%s': ",
                c->shift_reduce ? "SHIFT/REDUCE" : "REDUCE/REDUCE", c->state, g->names[c->term]);
        for (k = 0; k < c->nacts; k++) {
            action_str(c->acts[k], buf, sizeof buf);
            fprintf(out, "%s%s", k ? " vs " : "", buf);
        }
        action_str(c->chosen, buf, sizeof buf);
        fprintf(out, " (resolved as %s)\n      items: ", buf);
        for (k = 0; k < st->n; k++) {
            if (!item_involved(t, st->items[k], c->term)) continue;
            lr0_item_str(g, st->items[k], buf, sizeof buf);
            fprintf(out, "%s%s", first ? "" : "; ", buf);
            first = 0;
        }
        fprintf(out, "\n");
    }
}
