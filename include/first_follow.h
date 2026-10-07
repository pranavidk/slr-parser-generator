/*
 * Module 3a - FIRST and FOLLOW set computation
 * (Owner: Member 3 - Nitin, 24BCE2819)
 *
 * FOLLOW sets decide where reductions are placed in the SLR table - that is
 * what distinguishes SLR from plain LR(0). Both are computed by fixed-point
 * iteration and support epsilon-productions. Sets are 64-bit bitmasks
 * indexed by symbol id.
 */
#ifndef FIRST_FOLLOW_H
#define FIRST_FOLLOW_H

#include "grammar.h"

typedef unsigned long long SymSet;
#define SYMBIT(s) (1ULL << (s))

/* compile-time check: every symbol id must fit in one SymSet */
typedef char symset_fits_check[(MAX_SYMBOLS <= 64) ? 1 : -1];

typedef struct {
    SymSet first[MAX_SYMBOLS];
    int    nullable[MAX_SYMBOLS];   /* epsilon in FIRST(X) */
    SymSet follow[MAX_SYMBOLS];
} Sets;

void   compute_sets(const Grammar *g, Sets *s);
SymSet first_of_seq(const Sets *s, const int *syms, int n, int *nullable);
void   sets_print(const Grammar *g, const Sets *s, FILE *out);

#endif
