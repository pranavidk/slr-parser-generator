/*
 * Module 1 - Grammar Specification, Loading & Augmentation
 * (Owner: Member 1)
 *
 * Reads a context-free grammar from plain text, numbers the productions,
 * classifies symbols into terminals / nonterminals, augments the grammar
 * with S' -> S and checks for unreachable / non-productive nonterminals.
 *
 * Grammar file format
 *     # comment
 *     S -> id = E
 *     E -> E + T | T
 *     T -> T * F | F
 *     F -> ( E ) | id | num
 *
 *   - symbols are separated by whitespace
 *   - "->" (or the UTF-8 arrow) separates head and body, "|" separates alternatives
 *   - "epsilon", "eps" or the UTF-8 epsilon character denotes an empty body
 *   - the head of the first rule is the start symbol
 *   - any symbol that never appears as a head is a terminal
 */
#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <stddef.h>
#include <stdio.h>

#define MAX_SYMBOLS   64   /* terminals + nonterminals (FIRST/FOLLOW use 64-bit sets) */
#define MAX_PRODS     64
#define MAX_RHS       16
#define MAX_NAME      32
#define MAX_WARNINGS  16
#define END_MARKER    "$"
#define EPSILON_TEXT  "eps"

typedef struct {
    int head;
    int rhs[MAX_RHS];
    int len;
} Production;

typedef struct {
    char names[MAX_SYMBOLS][MAX_NAME];
    int  is_terminal[MAX_SYMBOLS];
    int  nsyms;

    Production prods[MAX_PRODS];      /* prods[0] is the augmented S' -> S */
    int nprods;

    int terms[MAX_SYMBOLS];           /* terminal ids, order of first appearance, '$' last */
    int nterms;
    int nonterms[MAX_SYMBOLS];        /* nonterminal ids, augmented start first */
    int nnonterms;

    int start;                        /* user start symbol S   */
    int aug_start;                    /* augmented start S'    */
    int end;                          /* end-of-input '$'      */

    char warnings[MAX_WARNINGS][128];
    int  nwarnings;
} Grammar;

/* Both return 0 on success, -1 on error (message written to err). */
int  grammar_from_text(Grammar *g, const char *text, char *err, size_t errlen);
int  grammar_from_file(Grammar *g, const char *path, char *err, size_t errlen);

int  grammar_find(const Grammar *g, const char *name);   /* symbol id or -1 */
void production_str(const Grammar *g, int prod, char *buf, size_t n);
void grammar_print(const Grammar *g, FILE *out);

/* Small shared utilities */
void  set_error(char *err, size_t errlen, const char *fmt, ...);
void  str_append(char *buf, size_t n, const char *fmt, ...);
char *read_text_file(const char *path);                  /* caller frees; NULL on failure */

#endif
