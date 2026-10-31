/*
 * Module 3b - SLR ACTION/GOTO Table Construction & Conflict Detection
 * (Owner: Member 3 - Nitin, 24BCE2819)
 *
 * Rules (Review 1, slide 7):
 *   Shift : A -> a.xb in I, x terminal, GOTO(I,x)=J  => ACTION[I,x] = sJ
 *   Reduce: A -> a. in I, A != S', x in FOLLOW(A)     => ACTION[I,x] = r(A->a)
 *   Accept: S' -> S. in I                             => ACTION[I,$] = acc
 *   GOTO  : GOTO(I,X)=J for nonterminal X             => GOTO[I,X]   = J
 *
 * When a cell receives two different actions the conflict is recorded
 * (state, terminal, kind, actions, items involved). The table still needs
 * one entry per cell, so the yacc convention is used: prefer shift on a
 * shift/reduce conflict, the lower-numbered production on reduce/reduce.
 * A grammar with any conflict is reported as NOT SLR(1).
 */
#ifndef SLR_TABLE_H
#define SLR_TABLE_H

#include "first_follow.h"
#include "lr0.h"

typedef enum { ACT_NONE = 0, ACT_SHIFT, ACT_REDUCE, ACT_ACCEPT } ActionKind;

typedef struct {
    ActionKind kind;
    int target;            /* state for shift, production index for reduce */
} Action;

#define MAX_CONFLICTS      128
#define MAX_CONFLICT_ACTS  8

typedef struct {
    int    state, term;
    int    shift_reduce;   /* 1 = shift/reduce, 0 = reduce/reduce */
    Action acts[MAX_CONFLICT_ACTS];
    int    nacts;
    Action chosen;
} Conflict;

typedef struct {
    const Grammar *g;
    const LR0     *lr;
    const Sets    *sets;
    Action action[MAX_STATES][MAX_SYMBOLS];
    int    go[MAX_STATES][MAX_SYMBOLS];       /* -1 = empty */
    Conflict conflicts[MAX_CONFLICTS];
    int    nconflicts;
} SLRTable;

/* Fill ACTION/GOTO from the LR(0) collection and FOLLOW sets and record any
 * conflicts. t keeps pointers to g, lr and sets, so they must outlive it. */
void table_build(SLRTable *t, const Grammar *g, const LR0 *lr, const Sets *sets);
int  table_is_slr(const SLRTable *t);    /* 1 if no conflicts were recorded */

/* Terminals with a non-empty ACTION entry in `state` (for error messages). */
int  table_expected(const SLRTable *t, int state, int *out);

/* "s<n>", "r<n>", "acc", or "" for an empty cell. */
void action_str(Action a, char *buf, size_t n);

/* ACTION/GOTO table as fixed-width text, one row per state. */
void table_print(const SLRTable *t, FILE *out);

/* One line per conflict (kind, state, terminal, competing actions, which
 * one was chosen) plus the items involved; or a "grammar is SLR(1)" line. */
void table_print_conflicts(const SLRTable *t, FILE *out);

/* ACTION/GOTO table as CSV (RFC 4180): header row "State", the terminals
 * (ACTION columns) then the nonterminals (GOTO columns); one row per state;
 * cells are s<n>, r<n>, acc, a GOTO state number, or empty. */
void table_write_csv(const SLRTable *t, FILE *out);

#endif
