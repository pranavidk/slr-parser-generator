/*
 * Module 5 - Syntax-Directed Translation & Three-Address Code
 * (Owner: Member 5)
 *
 * Every symbol on the value stack carries a synthesized attribute `place`
 * (the name or temporary holding its value). Actions run on each reduction
 * (Review 1, slide 9):
 *
 *   S -> id = E   : emit(id.lexeme = E.place)
 *   E -> E1 + T   : t = newTemp(); emit(t = E1.place + T.place); E.place = t
 *   E -> T        : E.place = T.place
 *   T -> T1 * F   : t = newTemp(); emit(t = T1.place * F.place); T.place = t
 *   T -> F        : T.place = F.place
 *   F -> ( E )    : F.place = E.place
 *   F -> id | num : F.place = lexeme
 *
 * Actions are chosen by the SHAPE of the production rather than its number,
 * so the translator keeps working if the grammar file is reordered or
 * extended with -, / or more precedence levels.
 *
 * Output is kept as quadruples (op, arg1, arg2, result) and printed as TAC.
 */
#ifndef SDT_H
#define SDT_H

#include "grammar.h"

#define MAX_PLACE 64
#define MAX_QUADS 1024

typedef struct {
    char op[8];
    char arg1[MAX_PLACE];
    char arg2[MAX_PLACE];
    char result[MAX_PLACE];
} Quad;

typedef struct {
    Quad quads[MAX_QUADS];
    int  nquads;
    int  temp_count;
} TACGen;

void tac_reset(TACGen *tg);

/* children[i] = place of the i-th RHS symbol. Writes the LHS place to out.
 * Returns 0, or -1 with a message in err (semantic error). */
int  tac_on_reduce(TACGen *tg, const Grammar *g, int prod,
                   char (*children)[MAX_PLACE], char *out, char *err, size_t errlen);

void quad_str(const Quad *q, char *buf, size_t n);
void tac_print_quads(const TACGen *tg, FILE *out);

#endif
