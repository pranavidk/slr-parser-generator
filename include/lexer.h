/*
 * Lexer - turns source text into the token stream the parser consumes.
 * (Shared utility, Owner: Member 1)
 *
 * Token classes are derived from the grammar's terminals, so the lexer
 * follows the grammar automatically:
 *   id   : [A-Za-z_][A-Za-z0-9_]*
 *   num  : digits with an optional fractional part
 *   other terminals ('=', '+', '*', '(', ')', ...) are matched literally,
 *   longest match first; a word equal to a terminal name is a keyword.
 *
 * Unknown characters produce a lexical error and are skipped, so lexing
 * never stops at the first bad character.
 */
#ifndef LEXER_H
#define LEXER_H

#include "grammar.h"

#define MAX_LEXEME 64
#define MAX_TOKENS 1024

typedef struct {
    int  kind;                 /* terminal symbol id */
    char lexeme[MAX_LEXEME];   /* exact source text   */
    int  line, col;            /* 1-based             */
} Token;

typedef struct {
    int  line, col;
    char message[128];
    char hint[128];
} LexError;

typedef struct {
    const Grammar *g;
    int id_sym, num_sym;                   /* -1 if the grammar has none */
    int keywords[MAX_SYMBOLS], nkeywords;
    int punct[MAX_SYMBOLS], npunct;        /* longest first */
} Lexer;

void lexer_init(Lexer *lx, const Grammar *g);

/* Returns the number of tokens written (always ends with '$'). */
int  lexer_tokenize(const Lexer *lx, const char *text, int line, int col_offset,
                    Token *toks, int max_toks, LexError *errs, int max_errs, int *nerrs);

/* "id(x)" for tokens whose lexeme differs from their kind, else just the kind */
void token_str(const Grammar *g, const Token *t, char *buf, size_t n);

#endif
