/*
 * Pipeline - wires the modules together into one parser generator.
 * (Owner: Member 4 - Shafin, 24BCE2860, shared)
 *
 *   Grammar -> LR(0) collection -> FIRST/FOLLOW -> SLR table -> Parser + SDT
 *
 * Statements are separated by newlines or ';'. Error recovery is
 * statement-level panic mode: an error is reported, the rest of that
 * statement is discarded (including any partial TAC) and parsing resumes
 * with the next statement, so one run reports every faulty statement.
 */
#ifndef PIPELINE_H
#define PIPELINE_H

#include "first_follow.h"
#include "grammar.h"
#include "lexer.h"
#include "lr0.h"
#include "parser.h"
#include "sdt.h"
#include "slr_table.h"

#define MAX_LINE        1024
#define MAX_STMT_DIAGS  8

typedef struct {
    Grammar  g;
    Lexer    lexer;
    LR0      lr;
    Sets     sets;
    SLRTable table;
    TACGen   tac;
} SLRGen;

SLRGen *gen_from_text(const char *grammar_text, char *err, size_t errlen);
SLRGen *gen_from_file(const char *path, char *err, size_t errlen);
void    gen_free(SLRGen *gen);

typedef struct {
    int  line;                 /* 1-based source line           */
    int  col_offset;           /* byte offset inside that line  */
    char text[MAX_LINE];
} Statement;

typedef struct {
    int  line;
    char source[MAX_LINE];     /* trimmed statement text        */
    int  accepted;
    int  quad_start, quad_end; /* TAC emitted by this statement */
    Diagnostic diags[MAX_STMT_DIAGS];
    int  ndiags;
    int  parsed;               /* 0 if lexing failed            */
    ParseResult parse;
} StatementResult;

typedef struct {
    StatementResult *stmts;
    int n, naccepted, nerrors;
} CompileResult;

/* Split source into statements. out == NULL just counts them. */
int  gen_split(const SLRGen *gen, const char *src, Statement *out, int max);

/* Lex + parse + translate one statement. trace may be NULL. */
void gen_compile_statement(SLRGen *gen, const Statement *st, FILE *trace, StatementResult *res);

/* Whole program (resets the TAC buffer first). 0 on success, -1 out of memory. */
int  gen_compile(SLRGen *gen, const char *src, CompileResult *out);
void compile_result_free(CompileResult *r);

/* Copy line `lineno` (1-based) of src into buf. Returns 0, or -1 if missing. */
int  get_source_line(const char *src, int lineno, char *buf, size_t n);

#endif
