/*
 * Module 4 - Shift-Reduce Parsing Engine & Error Diagnostics
 * (Owner: Member 4 - Shafin, 24BCE2860)
 *
 * Table-driven LR driver (Review 1, slide 8):
 *   stack = [0]
 *   loop: act = ACTION[top, lookahead]
 *     shift  -> push state + token attribute, advance
 *     reduce -> pop |rhs| entries, run semantic action, push GOTO[top, A]
 *     accept -> done
 *     error  -> build a diagnostic
 *
 * The state stack and a parallel value stack (semantic attributes) move
 * together. Each step can be printed as a trace.
 *
 * On error the diagnostic gives line/column, current state, offending
 * token, expected terminals (non-empty ACTION entries for that state) and a
 * targeted recovery hint.
 *
 * Note: SLR may perform a few reductions before detecting an error (FOLLOW
 * sets are coarser than exact lookaheads), but it never SHIFTS a bad token,
 * so the reported position is always exact.
 */
#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "sdt.h"
#include "slr_table.h"

#define MAX_STACK 512

typedef enum { DIAG_LEXICAL, DIAG_SYNTAX, DIAG_SEMANTIC } DiagKind;

typedef struct {
    DiagKind kind;
    int  line, col;
    char message[160];
    char hint[256];
    int  state;                    /* -1 if not applicable */
    int  expected[MAX_SYMBOLS];
    int  nexpected;
} Diagnostic;

typedef struct {
    int  accepted;
    int  nsteps;
    char last_action[64];
    char value[MAX_PLACE];         /* attribute of the start symbol */
    Diagnostic diag;               /* valid when !accepted */
} ParseResult;

/* trace may be NULL. Returns 1 if accepted. */
int  slr_parse(const SLRTable *t, TACGen *tac, const Token *toks, int ntok,
               FILE *trace, ParseResult *res);

/* source_line may be NULL (then no caret is drawn). */
void diag_print(const Grammar *g, const Diagnostic *d, const char *source_line, FILE *out);

#endif
