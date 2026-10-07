/*
 * Review 2 verification suite.
 *
 * Section A re-runs the 7 test cases from the Review 1 test plan (slide 10),
 * with concrete identifiers/numbers in place of id / num.
 * Sections B-F cover each module individually. Section G runs whole
 * programs through every grammar and checks the exact TAC or diagnostic.
 *
 * Build & run from the project root:   make test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline.h"

#ifndef GRAMMAR_DIR
#define GRAMMAR_DIR "grammars/"
#endif

static int tests_run, tests_failed, current_failed;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            current_failed = 1;                                             \
        }                                                                   \
    } while (0)

#define RUN(fn)                                                             \
    do {                                                                    \
        current_failed = 0;                                                 \
        fn();                                                               \
        tests_run++;                                                        \
        if (current_failed) tests_failed++;                                 \
        printf("%-52s %s\n", #fn, current_failed ? "FAIL" : "ok");          \
    } while (0)

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static SLRGen *load(const char *name)
{
    char path[256], err[256];
    SLRGen *g;
    snprintf(path, sizeof path, "%s%s.grammar", GRAMMAR_DIR, name);
    g = gen_from_file(path, err, sizeof err);
    if (!g) {
        printf("cannot load %s: %s\n", path, err);
        exit(2);
    }
    return g;
}

/* Compare all emitted TAC against an expected list */
static int tac_equals(const SLRGen *g, const char *const *expected, int n)
{
    char buf[256];
    int i;
    if (g->tac.nquads != n) return 0;
    for (i = 0; i < n; i++) {
        quad_str(&g->tac.quads[i], buf, sizeof buf);
        if (strcmp(buf, expected[i]) != 0) return 0;
    }
    return 1;
}

/* Compile one statement; returns its result (valid until next call) */
static StatementResult *one(SLRGen *g, const char *src)
{
    static CompileResult r;
    compile_result_free(&r);
    gen_compile(g, src, &r);
    return r.n > 0 ? &r.stmts[0] : NULL;
}

static int expects(const SLRGen *g, const Diagnostic *d, const char *term)
{
    int i, sym = grammar_find(&g->g, term);
    for (i = 0; i < d->nexpected; i++)
        if (d->expected[i] == sym) return 1;
    return 0;
}

static SymSet set_of(const SLRGen *g, const char *const *names)
{
    SymSet s = 0;
    for (; *names; names++) s |= SYMBIT(grammar_find(&g->g, *names));
    return s;
}

/* ------------------------------------------------------------------ */
/* A. Review 1 test plan (slide 10)                                    */
/* ------------------------------------------------------------------ */
static void test_01_valid_precedence(void)
{
    static const char *const exp[] = {"t1 = y * 2", "t2 = 5 + t1", "x = t2"};
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = 5 + y * 2");
    CHECK(st && st->accepted);
    CHECK(tac_equals(g, exp, 3));
    gen_free(g);
}

static void test_02_valid_parentheses_override(void)
{
    static const char *const exp[] = {"t1 = y + 5", "t2 = t1 * 2", "x = t2"};
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = (y + 5) * 2");
    CHECK(st && st->accepted);
    CHECK(tac_equals(g, exp, 3));
    gen_free(g);
}

static void test_03_edge_minimal(void)
{
    static const char *const exp[] = {"x = 7"};
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = 7");
    CHECK(st && st->accepted);
    CHECK(tac_equals(g, exp, 1));
    gen_free(g);
}

static void test_04_edge_nested_parentheses(void)
{
    static const char *const exp[] = {"x = 7"};
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = (((7)))");
    CHECK(st && st->accepted);
    CHECK(tac_equals(g, exp, 1));
    gen_free(g);
}

static void test_05_invalid_unexpected_plus(void)
{
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = + 7");
    CHECK(st && !st->accepted && st->ndiags == 1);
    CHECK(st->diags[0].kind == DIAG_SYNTAX && st->diags[0].col == 5);
    CHECK(strstr(st->diags[0].message, "unexpected '+'") != NULL);
    CHECK(strstr(st->diags[0].hint, "operand") != NULL);
    gen_free(g);
}

static void test_06_invalid_missing_close_paren(void)
{
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = (7 + y");
    CHECK(st && !st->accepted);
    CHECK(strstr(st->diags[0].message, "end of input") != NULL);
    CHECK(expects(g, &st->diags[0], ")"));
    CHECK(strstr(st->diags[0].hint, "missing ')'") != NULL);
    gen_free(g);
}

static void test_07_invalid_end_after_operator(void)
{
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = 7 *");
    CHECK(st && !st->accepted);
    CHECK(strstr(st->diags[0].message, "end of input") != NULL);
    CHECK(strstr(st->diags[0].hint, "operand is missing") != NULL);
    gen_free(g);
}

/* ------------------------------------------------------------------ */
/* B. Module 1 - grammar                                               */
/* ------------------------------------------------------------------ */
static void test_grammar_augmentation_and_symbols(void)
{
    SLRGen *g = load("assignment");
    char buf[128];
    production_str(&g->g, 0, buf, sizeof buf);
    CHECK(strcmp(buf, "S' -> S") == 0);
    CHECK(g->g.nprods == 9);
    CHECK(g->g.nterms == 8 && g->g.nnonterms == 5);
    CHECK(g->g.terms[g->g.nterms - 1] == g->g.end);
    CHECK(g->g.is_terminal[grammar_find(&g->g, "num")]);
    CHECK(!g->g.is_terminal[grammar_find(&g->g, "F")]);
    gen_free(g);
}

static void test_grammar_malformed_rejected(void)
{
    char err[256];
    CHECK(gen_from_text("S id = E", err, sizeof err) == NULL);
    CHECK(strstr(err, "missing '->'") != NULL);
    CHECK(gen_from_text("S -> a | | b", err, sizeof err) == NULL);
    CHECK(strstr(err, "empty alternative") != NULL);
    CHECK(gen_from_text("S -> a $", err, sizeof err) == NULL);
    CHECK(gen_from_text("# only a comment\n", err, sizeof err) == NULL);
}

static void test_grammar_unreachable_warned(void)
{
    char err[256];
    SLRGen *g = gen_from_text("S -> a\nX -> b", err, sizeof err);
    CHECK(g != NULL);
    CHECK(g && g->g.nwarnings == 1 && strstr(g->g.warnings[0], "unreachable") != NULL);
    gen_free(g);
}

static void test_grammar_utf8_arrow_and_epsilon(void)
{
    char err[256];
    SLRGen *g = gen_from_text("S \xE2\x86\x92 a B\nB \xE2\x86\x92 b | \xCE\xB5", err, sizeof err);
    CHECK(g != NULL);
    CHECK(g && g->g.nprods == 4 && g->g.prods[3].len == 0);
    CHECK(g && g->sets.nullable[grammar_find(&g->g, "B")]);
    gen_free(g);
}

/* ------------------------------------------------------------------ */
/* C. Module 2 - LR(0) collection                                      */
/* ------------------------------------------------------------------ */
static void test_lr0_state_count_matches_review1(void)
{
    SLRGen *g = load("assignment");
    CHECK(g->lr.nstates == 16);
    gen_free(g);
}

static void test_lr0_closure_of_start(void)
{
    SLRGen *g = load("assignment");
    char a[128], b[128];
    CHECK(g->lr.states[0].n == 2);
    lr0_item_str(&g->g, g->lr.states[0].items[0], a, sizeof a);
    lr0_item_str(&g->g, g->lr.states[0].items[1], b, sizeof b);
    CHECK(strcmp(a, "S' -> . S") == 0);
    CHECK(strcmp(b, "S -> . id = E") == 0);
    gen_free(g);
}

static void test_lr0_goto_is_deterministic(void)
{
    SLRGen *g = load("assignment");
    ItemSet tmp;
    int id = grammar_find(&g->g, "id");
    int target = g->lr.trans[0][id];
    lr0_goto(&g->g, &g->lr.states[0], id, &tmp);
    CHECK(target >= 0);
    CHECK(tmp.n == g->lr.states[target].n &&
          memcmp(tmp.items, g->lr.states[target].items, (size_t)tmp.n * sizeof(Item)) == 0);
    gen_free(g);
}

/* ------------------------------------------------------------------ */
/* D. Module 3 - FIRST/FOLLOW, table, conflicts                        */
/* ------------------------------------------------------------------ */
static void test_table_follow_sets(void)
{
    static const char *const fs[] = {"$", NULL};
    static const char *const fe[] = {"+", ")", "$", NULL};
    static const char *const ft[] = {"+", "*", ")", "$", NULL};
    SLRGen *g = load("assignment");
    const Sets *s = &g->sets;
    CHECK(s->follow[grammar_find(&g->g, "S")] == set_of(g, fs));
    CHECK(s->follow[grammar_find(&g->g, "E")] == set_of(g, fe));
    CHECK(s->follow[grammar_find(&g->g, "T")] == set_of(g, ft));
    CHECK(s->follow[grammar_find(&g->g, "F")] == set_of(g, ft));
    gen_free(g);
}

static void test_table_project_grammar_is_slr(void)
{
    SLRGen *g = load("assignment");
    CHECK(table_is_slr(&g->table));
    gen_free(g);
}

static void test_table_accept_entry(void)
{
    SLRGen *g = load("assignment");
    CHECK(g->table.action[1][g->g.end].kind == ACT_ACCEPT);
    gen_free(g);
}

static void test_table_ambiguous_conflicts_detected(void)
{
    SLRGen *g = load("ambiguous");
    int i;
    CHECK(!table_is_slr(&g->table));
    CHECK(g->table.nconflicts == 4);
    for (i = 0; i < g->table.nconflicts; i++) CHECK(g->table.conflicts[i].shift_reduce);
    gen_free(g);
}

static void test_table_reduce_reduce_detected(void)
{
    char err[256];
    SLRGen *g = gen_from_text("S -> A | B\nA -> x\nB -> x", err, sizeof err);
    int i, found = 0;
    CHECK(g != NULL);
    for (i = 0; g && i < g->table.nconflicts; i++)
        if (!g->table.conflicts[i].shift_reduce) found = 1;
    CHECK(found);
    gen_free(g);
}

/* ------------------------------------------------------------------ */
/* E. Module 4 - parser driver & error recovery                        */
/* ------------------------------------------------------------------ */
static void test_parser_trace_ends_with_accept(void)
{
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = a");
    CHECK(st && st->parsed && strcmp(st->parse.last_action, "ACCEPT") == 0);
    CHECK(st && st->parse.nsteps == 8);
    gen_free(g);
}

static void test_parser_missing_operator(void)
{
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = a b");
    CHECK(st && strstr(st->diags[0].hint, "operator is probably missing") != NULL);
    gen_free(g);
}

static void test_parser_unmatched_close_paren(void)
{
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = a )");
    CHECK(st && strstr(st->diags[0].hint, "Unmatched ')'") != NULL);
    gen_free(g);
}

static void test_parser_empty_parentheses(void)
{
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = ()");
    CHECK(st && strstr(st->diags[0].hint, "Empty parentheses") != NULL);
    gen_free(g);
}

static void test_parser_statement_must_start_with_id(void)
{
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "5 = x");
    CHECK(st && st->diags[0].col == 1);
    CHECK(st && strstr(st->diags[0].hint, "must start with") != NULL);
    gen_free(g);
}

static void test_parser_lexical_error(void)
{
    SLRGen *g = load("assignment");
    StatementResult *st = one(g, "x = a @ b");
    CHECK(st && !st->accepted && !st->parsed);
    CHECK(st && st->diags[0].kind == DIAG_LEXICAL && st->diags[0].col == 7);
    gen_free(g);
}

static void test_parser_recovery_continues_after_error(void)
{
    static const char *const exp[] = {"a = 1", "c = 3"};
    SLRGen *g = load("assignment");
    CompileResult r;
    gen_compile(g, "a = 1\nb = + 2\nc = 3; d = (4", &r);
    CHECK(r.n == 4);
    CHECK(r.n == 4 && r.stmts[0].accepted && !r.stmts[1].accepted &&
          r.stmts[2].accepted && !r.stmts[3].accepted);
    CHECK(r.nerrors == 2);
    CHECK(r.n == 4 && r.stmts[3].line == 3 && r.stmts[3].diags[0].col == 14);
    CHECK(tac_equals(g, exp, 2));
    compile_result_free(&r);
    gen_free(g);
}

static void test_parser_deep_nesting_is_reported(void)
{
    SLRGen *g = load("assignment");
    char src[MAX_LINE];
    int i, n = 0;
    StatementResult *st;
    n += snprintf(src + n, sizeof src - (size_t)n, "x = ");
    for (i = 0; i < 600; i++) src[n++] = '(';   /* deeper than MAX_STACK */
    src[n++] = '1';
    src[n] = '\0';
    st = one(g, src);
    CHECK(st && !st->accepted && st->diags[0].kind == DIAG_SEMANTIC);
    CHECK(st && strstr(st->diags[0].message, "nested too deeply") != NULL);
    gen_free(g);
}

/* ------------------------------------------------------------------ */
/* F. Module 5 - SDT / TAC                                             */
/* ------------------------------------------------------------------ */
static void test_tac_slide9_example(void)
{
    static const char *const exp[] = {"t1 = b * c", "t2 = a + t1", "x = t2"};
    SLRGen *g = load("assignment");
    one(g, "x = a + b * c");
    CHECK(tac_equals(g, exp, 3));
    gen_free(g);
}

static void test_tac_left_associativity(void)
{
    static const char *const exp[] = {"t1 = a + b", "t2 = t1 + c", "x = t2"};
    SLRGen *g = load("assignment");
    one(g, "x = a + b + c");
    CHECK(tac_equals(g, exp, 3));
    gen_free(g);
}

static void test_tac_temps_continue_across_statements(void)
{
    static const char *const exp[] = {"t1 = a * b", "x = t1", "t2 = x + c", "y = t2"};
    SLRGen *g = load("assignment");
    one(g, "x = a * b\ny = x + c");
    CHECK(tac_equals(g, exp, 4));
    gen_free(g);
}

static void test_tac_rejected_statement_emits_no_code(void)
{
    static const char *const exp[] = {"t1 = c * d", "y = t1"};
    SLRGen *g = load("assignment");
    one(g, "x = a * b + \ny = c * d");
    CHECK(tac_equals(g, exp, 2));
    gen_free(g);
}

static void test_tac_extended_grammar_without_code_changes(void)
{
    static const char *const exp[] = {"t1 = b / c", "t2 = a - t1", "r = t2"};
    SLRGen *g = load("extended");
    StatementResult *st = one(g, "r = a - b / c");
    CHECK(table_is_slr(&g->table));
    CHECK(st && st->accepted);
    CHECK(tac_equals(g, exp, 3));
    gen_free(g);
}

static void test_tac_quadruples(void)
{
    SLRGen *g = load("assignment");
    const Quad *q = g->tac.quads;
    one(g, "x = a + b");
    CHECK(g->tac.nquads == 2);
    CHECK(!strcmp(q[0].op, "+") && !strcmp(q[0].arg1, "a") && !strcmp(q[0].arg2, "b") &&
          !strcmp(q[0].result, "t1"));
    CHECK(!strcmp(q[1].op, "=") && !strcmp(q[1].arg1, "t1") && !strcmp(q[1].result, "x"));
    gen_free(g);
}

/* ------------------------------------------------------------------ */
/* G. End-to-end cases (expected output derived by hand)               */
/* ------------------------------------------------------------------ */
typedef struct {
    const char *grammar, *src;
    const char *tac[6];                 /* NULL-terminated */
} TacCase;

typedef struct {
    const char *grammar, *src;
    DiagKind kind;
    int col;
    const char *message;                /* substring of the diagnostic */
} ErrCase;

static void test_e2e_accepted_programs(void)
{
    static const TacCase cases[] = {
        {"assignment", "a = b",                 {"a = b"}},
        {"assignment", "x = a * b * c",         {"t1 = a * b", "t2 = t1 * c", "x = t2"}},
        {"assignment", "x = a * b + c * d",     {"t1 = a * b", "t2 = c * d", "t3 = t1 + t2", "x = t3"}},
        {"assignment", "x = a * (b + c)",       {"t1 = b + c", "t2 = a * t1", "x = t2"}},
        {"assignment", "p = a + b * c + d",     {"t1 = b * c", "t2 = a + t1", "t3 = t2 + d", "p = t3"}},
        {"assignment", "q = (a + b) * (c + d)", {"t1 = a + b", "t2 = c + d", "t3 = t1 * t2", "q = t3"}},
        {"assignment", "x = 2 * 3 + 4",         {"t1 = 2 * 3", "t2 = t1 + 4", "x = t2"}},
        {"assignment", "x = ((a))",             {"x = a"}},
        {"extended",   "r = a / b * c",         {"t1 = a / b", "t2 = t1 * c", "r = t2"}},
        {"extended",   "r = a - b + c",         {"t1 = a - b", "t2 = t1 + c", "r = t2"}},
        {"extended",   "r = a - (b - c)",       {"t1 = b - c", "t2 = a - t1", "r = t2"}},
        {"extended",   "r = a + b / c - d * e", {"t1 = b / c", "t2 = a + t1", "t3 = d * e", "t4 = t2 - t3", "r = t4"}},
        /* shift wins every conflict, so the later operator binds tighter */
        {"ambiguous",  "x = a * b + c",         {"t1 = b + c", "t2 = a * t1", "x = t2"}},
    };
    size_t i;
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const TacCase *c = &cases[i];
        SLRGen *g = load(c->grammar);
        StatementResult *st = one(g, c->src);
        int n = 0;
        while (n < 6 && c->tac[n]) n++;
        if (!(st && st->accepted && tac_equals(g, c->tac, n)))
            printf("    case: %s  [%s]\n", c->src, c->grammar);
        CHECK(st && st->accepted);
        CHECK(tac_equals(g, c->tac, n));
        gen_free(g);
    }
}

static void test_e2e_rejected_programs(void)
{
    static const ErrCase cases[] = {
        {"assignment", "x = (a + b))", DIAG_SYNTAX,  12, "unexpected ')'"},
        {"assignment", "x = a + * b",  DIAG_SYNTAX,   9, "unexpected '*'"},
        {"assignment", "x = a +",      DIAG_SYNTAX,   8, "end of input"},
        {"assignment", "x a + b",      DIAG_SYNTAX,   3, "unexpected 'a'"},
        {"assignment", "= a + b",      DIAG_SYNTAX,   1, "unexpected '='"},
        {"assignment", "x = a - b",    DIAG_LEXICAL,  7, "'-'"},
        {"extended",   "x = a // b",   DIAG_SYNTAX,   8, "unexpected '/'"},
    };
    size_t i;
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const ErrCase *c = &cases[i];
        SLRGen *g = load(c->grammar);
        StatementResult *st = one(g, c->src);
        int ok = st && !st->accepted && st->ndiags >= 1 &&
                 st->diags[0].kind == c->kind && st->diags[0].col == c->col &&
                 strstr(st->diags[0].message, c->message) != NULL;
        if (!ok) printf("    case: %s  [%s]\n", c->src, c->grammar);
        CHECK(ok);
        CHECK(g->tac.nquads == 0);
        gen_free(g);
    }
}

/* ------------------------------------------------------------------ */
int main(void)
{
    printf("A. Review 1 test plan (slide 10)\n");
    RUN(test_01_valid_precedence);
    RUN(test_02_valid_parentheses_override);
    RUN(test_03_edge_minimal);
    RUN(test_04_edge_nested_parentheses);
    RUN(test_05_invalid_unexpected_plus);
    RUN(test_06_invalid_missing_close_paren);
    RUN(test_07_invalid_end_after_operator);

    printf("\nB. Module 1 - grammar\n");
    RUN(test_grammar_augmentation_and_symbols);
    RUN(test_grammar_malformed_rejected);
    RUN(test_grammar_unreachable_warned);
    RUN(test_grammar_utf8_arrow_and_epsilon);

    printf("\nC. Module 2 - LR(0) collection\n");
    RUN(test_lr0_state_count_matches_review1);
    RUN(test_lr0_closure_of_start);
    RUN(test_lr0_goto_is_deterministic);

    printf("\nD. Module 3 - FIRST/FOLLOW and SLR table\n");
    RUN(test_table_follow_sets);
    RUN(test_table_project_grammar_is_slr);
    RUN(test_table_accept_entry);
    RUN(test_table_ambiguous_conflicts_detected);
    RUN(test_table_reduce_reduce_detected);

    printf("\nE. Module 4 - parser and error recovery\n");
    RUN(test_parser_trace_ends_with_accept);
    RUN(test_parser_missing_operator);
    RUN(test_parser_unmatched_close_paren);
    RUN(test_parser_empty_parentheses);
    RUN(test_parser_statement_must_start_with_id);
    RUN(test_parser_lexical_error);
    RUN(test_parser_recovery_continues_after_error);
    RUN(test_parser_deep_nesting_is_reported);

    printf("\nF. Module 5 - SDT / three-address code\n");
    RUN(test_tac_slide9_example);
    RUN(test_tac_left_associativity);
    RUN(test_tac_temps_continue_across_statements);
    RUN(test_tac_rejected_statement_emits_no_code);
    RUN(test_tac_extended_grammar_without_code_changes);
    RUN(test_tac_quadruples);

    printf("\nG. End-to-end cases\n");
    RUN(test_e2e_accepted_programs);
    RUN(test_e2e_rejected_programs);

    printf("\n%d tests, %d passed, %d failed\n", tests_run, tests_run - tests_failed, tests_failed);
    return tests_failed ? 1 : 0;
}
