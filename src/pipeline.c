/*
 * Pipeline (Owner: Member 4, shared)
 */
#include "pipeline.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static SLRGen *finish(SLRGen *gen, char *err, size_t errlen)
{
    if (lr0_build(&gen->lr, &gen->g, err, errlen) < 0) {
        free(gen);
        return NULL;
    }
    compute_sets(&gen->g, &gen->sets);
    table_build(&gen->table, &gen->g, &gen->lr, &gen->sets);
    lexer_init(&gen->lexer, &gen->g);
    tac_reset(&gen->tac);
    return gen;
}

SLRGen *gen_from_text(const char *grammar_text, char *err, size_t errlen)
{
    SLRGen *gen = calloc(1, sizeof *gen);
    if (!gen) {
        set_error(err, errlen, "Out of memory.");
        return NULL;
    }
    if (grammar_from_text(&gen->g, grammar_text, err, errlen) < 0) {
        free(gen);
        return NULL;
    }
    return finish(gen, err, errlen);
}

SLRGen *gen_from_file(const char *path, char *err, size_t errlen)
{
    SLRGen *gen = calloc(1, sizeof *gen);
    if (!gen) {
        set_error(err, errlen, "Out of memory.");
        return NULL;
    }
    if (grammar_from_file(&gen->g, path, err, errlen) < 0) {
        free(gen);
        return NULL;
    }
    return finish(gen, err, errlen);
}

void gen_free(SLRGen *gen) { free(gen); }

/* ------------------------------------------------------------------ */
static int blank(const char *s, const char *e)
{
    for (; s < e; s++)
        if (!isspace((unsigned char)*s)) return 0;
    return 1;
}

int gen_split(const SLRGen *gen, const char *src, Statement *out, int max)
{
    int use_semicolon = grammar_find(&gen->g, ";") < 0;
    int count = 0, line = 1;
    const char *p = src;

    while (*p) {
        const char *eol = strchr(p, '\n');
        const char *line_end = eol ? eol : p + strlen(p);
        const char *s = p;

        while (s <= line_end) {
            const char *e = s;
            if (use_semicolon)
                while (e < line_end && *e != ';') e++;
            else
                e = line_end;
            if (!blank(s, e)) {
                if (out) {
                    Statement *st;
                    size_t len = (size_t)(e - s);
                    if (count >= max) return count;
                    st = &out[count];
                    if (len >= MAX_LINE) len = MAX_LINE - 1;
                    st->line = line;
                    st->col_offset = (int)(s - p);
                    memcpy(st->text, s, len);
                    st->text[len] = '\0';
                }
                count++;
            }
            s = e + 1;
        }
        if (!eol) break;
        p = eol + 1;
        line++;
    }
    return count;
}

static void trimmed_copy(char *dst, size_t n, const char *src)
{
    const char *e;
    size_t len;
    while (isspace((unsigned char)*src)) src++;
    e = src + strlen(src);
    while (e > src && isspace((unsigned char)e[-1])) e--;
    len = (size_t)(e - src);
    if (len >= n) len = n - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

void gen_compile_statement(SLRGen *gen, const Statement *st, FILE *trace, StatementResult *res)
{
    static Token toks[MAX_TOKENS];
    LexError errs[MAX_STMT_DIAGS];
    int ntok, nerrs, i, mark, temps;

    memset(res, 0, sizeof *res);
    res->line = st->line;
    trimmed_copy(res->source, sizeof res->source, st->text);
    res->quad_start = res->quad_end = gen->tac.nquads;

    ntok = lexer_tokenize(&gen->lexer, st->text, st->line, st->col_offset, toks, MAX_TOKENS,
                          errs, MAX_STMT_DIAGS, &nerrs);
    if (nerrs > 0) {
        for (i = 0; i < nerrs; i++) {
            Diagnostic *d = &res->diags[res->ndiags++];
            d->kind = DIAG_LEXICAL;
            d->line = errs[i].line;
            d->col = errs[i].col;
            d->state = -1;
            snprintf(d->message, sizeof d->message, "%s", errs[i].message);
            snprintf(d->hint, sizeof d->hint, "%s", errs[i].hint);
        }
        return;
    }

    mark = gen->tac.nquads;
    temps = gen->tac.temp_count;
    res->parsed = 1;
    slr_parse(&gen->table, &gen->tac, toks, ntok, trace, &res->parse);
    if (res->parse.accepted) {
        res->accepted = 1;
        res->quad_end = gen->tac.nquads;
    } else {
        gen->tac.nquads = mark;              /* discard partial code */
        gen->tac.temp_count = temps;
        res->diags[res->ndiags++] = res->parse.diag;
    }
}

int gen_compile(SLRGen *gen, const char *src, CompileResult *out)
{
    Statement *stmts;
    int n, i;

    memset(out, 0, sizeof *out);
    tac_reset(&gen->tac);
    n = gen_split(gen, src, NULL, 0);
    if (n == 0) return 0;

    stmts = malloc((size_t)n * sizeof *stmts);
    out->stmts = calloc((size_t)n, sizeof *out->stmts);
    if (!stmts || !out->stmts) {
        free(stmts);
        free(out->stmts);
        out->stmts = NULL;
        return -1;
    }
    out->n = gen_split(gen, src, stmts, n);
    for (i = 0; i < out->n; i++) {
        gen_compile_statement(gen, &stmts[i], NULL, &out->stmts[i]);
        out->naccepted += out->stmts[i].accepted;
        out->nerrors += out->stmts[i].ndiags;
    }
    free(stmts);
    return 0;
}

void compile_result_free(CompileResult *r)
{
    free(r->stmts);
    r->stmts = NULL;
    r->n = 0;
}

int get_source_line(const char *src, int lineno, char *buf, size_t n)
{
    const char *p = src, *eol;
    size_t len;
    int i;
    for (i = 1; i < lineno; i++) {
        p = strchr(p, '\n');
        if (!p) return -1;
        p++;
    }
    eol = strchr(p, '\n');
    len = eol ? (size_t)(eol - p) : strlen(p);
    if (len > 0 && p[len - 1] == '\r') len--;
    if (len >= n) len = n - 1;
    memcpy(buf, p, len);
    buf[len] = '\0';
    return 0;
}
