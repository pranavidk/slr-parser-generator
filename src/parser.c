/*
 * Module 4 - Shift-reduce driver, trace & diagnostics (Owner: Member 4 - Shafin, 24BCE2860)
 */
#include "parser.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static int named(const Grammar *g, int sym, const char *name)
{
    return sym >= 0 && strcmp(g->names[sym], name) == 0;
}

static int is_operand_start(const Grammar *g, int sym)
{
    return named(g, sym, "id") || named(g, sym, "num") || named(g, sym, "(");
}

static void list_terms(const Grammar *g, const int *syms, int n, char *buf, size_t sz)
{
    int i;
    buf[0] = '\0';
    for (i = 0; i < n; i++) {
        const char *sep = i == 0 ? "" : (i == n - 1 ? " or " : ", ");
        if (syms[i] == g->end) str_append(buf, sz, "%send of input", sep);
        else str_append(buf, sz, "%s'%s'", sep, g->names[syms[i]]);
    }
}

typedef struct {
    int states[MAX_STACK];
    int syms[MAX_STACK];          /* grammar symbol under each state (index 0 unused) */
    int tok[MAX_STACK];           /* token index for terminals, -1 for nonterminals */
    char values[MAX_STACK][MAX_PLACE];
    int sp;                       /* index of top */
} Stack;

static void trace_row(FILE *out, const Grammar *g, const Stack *s, const Token *toks,
                      int ntok, int pos, int step, const char *action)
{
    char st[2048] = "", sy[2048] = "", in[2048] = "", tb[128];
    int i;
    if (!out) return;
    for (i = 0; i <= s->sp; i++) str_append(st, sizeof st, "%s%d", i ? " " : "", s->states[i]);
    for (i = 1; i <= s->sp; i++) {
        if (s->tok[i] >= 0) token_str(g, &toks[s->tok[i]], tb, sizeof tb);
        else snprintf(tb, sizeof tb, "%s", g->names[s->syms[i]]);
        str_append(sy, sizeof sy, "%s%s", i > 1 ? " " : "", tb);
    }
    for (i = pos; i < ntok; i++) {
        token_str(g, &toks[i], tb, sizeof tb);
        str_append(in, sizeof in, "%s%s", i > pos ? " " : "", tb);
    }
    fprintf(out, "%-5d %-24s %-24s %-34s %s\n", step, st, sy, in, action);
}

static void make_hint(const Grammar *g, const Token *tok, const int *exp, int nexp,
                      const Stack *s, const Token *toks, char *hint, size_t n)
{
    int wants_operand = 0, wants_close = 0, i;
    int is_operand = is_operand_start(g, tok->kind);
    char list[256];

    for (i = 0; i < nexp; i++) {
        if (is_operand_start(g, exp[i])) wants_operand = 1;
        if (named(g, exp[i], ")")) wants_close = 1;
    }
    list_terms(g, exp, nexp, list, sizeof list);

    if (s->sp == 0) {
        snprintf(hint, n, "A statement must start with %s.", list);
    } else if (tok->kind == g->end) {
        if (wants_close) {
            snprintf(hint, n, "Input ended while a '(' is still open - add the missing ')'.");
        } else if (wants_operand) {
            const char *last = s->tok[s->sp] >= 0 ? toks[s->tok[s->sp]].lexeme
                                                  : g->names[s->syms[s->sp]];
            snprintf(hint, n, "Input ended right after '%s' - an operand is missing.", last);
        } else {
            snprintf(hint, n, "Statement is incomplete; it can continue with %s.", list);
        }
    } else if (named(g, tok->kind, ")") && wants_operand) {
        snprintf(hint, n, "Empty parentheses or a missing operand before ')'.");
    } else if (named(g, tok->kind, ")") && !wants_close) {
        snprintf(hint, n, "Unmatched ')' - there is no open '(' for it.");
    } else if (wants_operand && !is_operand) {
        snprintf(hint, n, "Operator '%s' appears where an operand was expected - "
                          "check for a missing operand or a doubled operator.", tok->lexeme);
    } else if (is_operand && !wants_operand) {
        snprintf(hint, n, "'%s' follows another operand directly - "
                          "an operator is probably missing between them.", tok->lexeme);
    } else {
        snprintf(hint, n, "Expected %s.", list);
    }
}

static void fail(ParseResult *r, DiagKind kind, const Token *tok, const char *msg)
{
    memset(&r->diag, 0, sizeof r->diag);
    r->accepted = 0;
    r->diag.kind = kind;
    r->diag.line = tok->line;
    r->diag.col = tok->col;
    r->diag.state = -1;
    snprintf(r->diag.message, sizeof r->diag.message, "%s", msg);
}

/* ------------------------------------------------------------------ */
/* Driver                                                              */
/* ------------------------------------------------------------------ */
int slr_parse(const SLRTable *t, TACGen *tac, const Token *toks, int ntok,
              FILE *trace, ParseResult *r)
{
    static Stack s;               /* ~40 KB: kept off the call stack */
    const Grammar *g = t->g;
    char desc[320], pbuf[256], err[256];
    int pos = 0;

    memset(r, 0, sizeof *r);
    s.sp = 0;
    s.states[0] = 0;
    if (trace)
        fprintf(trace, "%-5s %-24s %-24s %-34s %s\n", "Step", "Stack (states)", "Symbols",
                "Input", "Action");

    for (;;) {
        const Token *tok = &toks[pos < ntok ? pos : ntok - 1];
        int top = s.states[s.sp];
        Action a = t->action[top][tok->kind];
        r->nsteps++;

        switch (a.kind) {
        case ACT_NONE: {
            trace_row(trace, g, &s, toks, ntok, pos, r->nsteps, "ERROR");
            snprintf(desc, sizeof desc, "unexpected %s%s%s",
                     tok->kind == g->end ? "end of input" : "'",
                     tok->kind == g->end ? "" : tok->lexeme,
                     tok->kind == g->end ? "" : "'");
            fail(r, DIAG_SYNTAX, tok, desc);
            r->diag.state = top;
            r->diag.nexpected = table_expected(t, top, r->diag.expected);
            make_hint(g, tok, r->diag.expected, r->diag.nexpected, &s, toks,
                      r->diag.hint, sizeof r->diag.hint);
            snprintf(r->last_action, sizeof r->last_action, "ERROR");
            return 0;
        }

        case ACT_SHIFT:
            snprintf(desc, sizeof desc, "shift %d", a.target);
            trace_row(trace, g, &s, toks, ntok, pos, r->nsteps, desc);
            if (s.sp + 1 >= MAX_STACK) {
                fail(r, DIAG_SEMANTIC, tok, "expression is nested too deeply");
                return 0;
            }
            s.sp++;
            s.states[s.sp] = a.target;
            s.syms[s.sp] = tok->kind;
            s.tok[s.sp] = pos;
            snprintf(s.values[s.sp], MAX_PLACE, "%s", tok->lexeme);
            pos++;
            break;

        case ACT_REDUCE: {
            const Production *p = &g->prods[a.target];
            char res[MAX_PLACE];
            int base = s.sp - p->len + 1;          /* first RHS entry */
            production_str(g, a.target, pbuf, sizeof pbuf);
            snprintf(desc, sizeof desc, "reduce %s", pbuf);
            trace_row(trace, g, &s, toks, ntok, pos, r->nsteps, desc);
            if (base >= MAX_STACK) {
                fail(r, DIAG_SEMANTIC, tok, "expression is nested too deeply");
                return 0;
            }
            if (tac_on_reduce(tac, g, a.target, &s.values[base], res, err, sizeof err) < 0) {
                fail(r, DIAG_SEMANTIC, tok, err);
                return 0;
            }
            s.sp -= p->len;
            s.sp++;
            s.states[s.sp] = t->go[s.states[s.sp - 1]][p->head];
            s.syms[s.sp] = p->head;
            s.tok[s.sp] = -1;
            snprintf(s.values[s.sp], MAX_PLACE, "%s", res);
            break;
        }

        case ACT_ACCEPT:
            trace_row(trace, g, &s, toks, ntok, pos, r->nsteps, "ACCEPT");
            r->accepted = 1;
            snprintf(r->value, sizeof r->value, "%s", s.values[s.sp]);
            snprintf(r->last_action, sizeof r->last_action, "ACCEPT");
            return 1;
        }
        snprintf(r->last_action, sizeof r->last_action, "%.63s", desc);
    }
}

void diag_print(const Grammar *g, const Diagnostic *d, const char *source_line, FILE *out)
{
    static const char *const kinds[] = {"Lexical", "Syntax", "Semantic"};
    char list[512] = "";
    int i;

    fprintf(out, "%s error at line %d, col %d: %s\n", kinds[d->kind], d->line, d->col, d->message);
    if (source_line) {
        int slen = (int)strlen(source_line);
        fprintf(out, "    %s\n    ", source_line);
        for (i = 1; i < d->col; i++)
            fputc(i - 1 < slen && source_line[i - 1] == '\t' ? '\t' : ' ', out);
        fprintf(out, "^\n");
    }
    if (d->state >= 0) fprintf(out, "    parser state : I%d\n", d->state);
    if (d->nexpected > 0) {
        for (i = 0; i < d->nexpected; i++)
            str_append(list, sizeof list, "%s'%s'", i ? ", " : "", g->names[d->expected[i]]);
        fprintf(out, "    expected     : %s\n", list);
    }
    if (d->hint[0]) fprintf(out, "    hint         : %s\n", d->hint);
}
