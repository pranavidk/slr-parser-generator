/*
 * Lexer (Owner: Member 1)
 */
#include "lexer.h"

#include <ctype.h>
#include <string.h>

static int is_ident_start(int c) { return isalpha(c) || c == '_'; }
static int is_ident_char(int c) { return isalnum(c) || c == '_'; }

static int is_ident_name(const char *s)
{
    if (!is_ident_start((unsigned char)*s)) return 0;
    for (s++; *s; s++)
        if (!is_ident_char((unsigned char)*s)) return 0;
    return 1;
}

void lexer_init(Lexer *lx, const Grammar *g)
{
    int i, j;
    memset(lx, 0, sizeof *lx);
    lx->g = g;
    lx->id_sym = lx->num_sym = -1;

    for (i = 0; i < g->nterms; i++) {
        int t = g->terms[i];
        const char *name = g->names[t];
        if (t == g->end) continue;
        if (strcmp(name, "id") == 0) lx->id_sym = t;
        else if (strcmp(name, "num") == 0) lx->num_sym = t;
        else if (is_ident_name(name)) lx->keywords[lx->nkeywords++] = t;
        else lx->punct[lx->npunct++] = t;
    }
    /* Longest punctuation first so that e.g. ":=" wins over ":" */
    for (i = 1; i < lx->npunct; i++) {
        int t = lx->punct[i];
        size_t len = strlen(g->names[t]);
        for (j = i; j > 0 && strlen(g->names[lx->punct[j - 1]]) < len; j--)
            lx->punct[j] = lx->punct[j - 1];
        lx->punct[j] = t;
    }
}

static const char *const HINT_UNKNOWN = "Remove it, or add it to the grammar as a terminal.";
static const char *const HINT_TOO_LONG = "Shorten it (limit is MAX_LEXEME - 1 characters).";
static const char *const HINT_TOO_MANY = "Split the statement, or raise MAX_TOKENS in lexer.h.";

static void add_error(LexError *errs, int max, int *n, int line, int col,
                      const char *what, const char *text, size_t len, const char *hint)
{
    char shown[MAX_LEXEME];
    if (*n >= max) return;
    if (len >= sizeof shown) len = sizeof shown - 1;
    memcpy(shown, text, len);
    shown[len] = '\0';
    errs[*n].line = line;
    errs[*n].col = col;
    snprintf(errs[*n].message, sizeof errs[*n].message, "%s '%s'", what, shown);
    snprintf(errs[*n].hint, sizeof errs[*n].hint, "%s", hint);
    (*n)++;
}

static void push(Token *toks, int *n, int kind, const char *text, size_t len, int line, int col)
{
    Token *t = &toks[(*n)++];
    t->kind = kind;
    memcpy(t->lexeme, text, len);
    t->lexeme[len] = '\0';
    t->line = line;
    t->col = col;
}

int lexer_tokenize(const Lexer *lx, const char *text, int line, int col_offset,
                   Token *toks, int max_toks, LexError *errs, int max_errs, int *nerrs)
{
    const Grammar *g = lx->g;
    size_t i = 0, len = strlen(text);
    int n = 0, k;

    *nerrs = 0;
    while (i < len) {
        unsigned char c = (unsigned char)text[i];
        int col = (int)i + 1 + col_offset;
        size_t j;

        if (isspace(c)) {
            i++;
            continue;
        }
        if (n >= max_toks - 1) {
            add_error(errs, max_errs, nerrs, line, col, "too many tokens in one statement near",
                      text + i, 1, HINT_TOO_MANY);
            break;
        }

        /* identifier or keyword */
        if (is_ident_start(c)) {
            int kw = -1;
            for (j = i; j < len && is_ident_char((unsigned char)text[j]); j++) {}
            if (j - i >= MAX_LEXEME) {
                add_error(errs, max_errs, nerrs, line, col, "identifier too long:", text + i, j - i,
                          HINT_TOO_LONG);
            } else {
                for (k = 0; k < lx->nkeywords; k++) {
                    const char *name = g->names[lx->keywords[k]];
                    if (strlen(name) == j - i && strncmp(name, text + i, j - i) == 0)
                        kw = lx->keywords[k];
                }
                if (kw >= 0) push(toks, &n, kw, text + i, j - i, line, col);
                else if (lx->id_sym >= 0) push(toks, &n, lx->id_sym, text + i, j - i, line, col);
                else add_error(errs, max_errs, nerrs, line, col, "unexpected identifier",
                               text + i, j - i, HINT_UNKNOWN);
            }
            i = j;
            continue;
        }

        /* number: digits ( . digits )? */
        if (isdigit(c)) {
            for (j = i; j < len && isdigit((unsigned char)text[j]); j++) {}
            if (j + 1 < len && text[j] == '.' && isdigit((unsigned char)text[j + 1]))
                for (j++; j < len && isdigit((unsigned char)text[j]); j++) {}
            if (j - i >= MAX_LEXEME)
                add_error(errs, max_errs, nerrs, line, col, "number too long:", text + i, j - i,
                          HINT_TOO_LONG);
            else if (lx->num_sym >= 0)
                push(toks, &n, lx->num_sym, text + i, j - i, line, col);
            else
                add_error(errs, max_errs, nerrs, line, col, "unexpected number", text + i, j - i,
                          HINT_UNKNOWN);
            i = j;
            continue;
        }

        /* punctuation terminal, longest match first */
        for (k = 0; k < lx->npunct; k++) {
            const char *name = g->names[lx->punct[k]];
            size_t pl = strlen(name);
            if (strncmp(text + i, name, pl) == 0) {
                push(toks, &n, lx->punct[k], name, pl, line, col);
                i += pl;
                break;
            }
        }
        if (k < lx->npunct) continue;

        /* unknown character (keep a whole UTF-8 sequence together) */
        j = i + 1;
        if (c >= 0x80)
            while (j < len && ((unsigned char)text[j] & 0xC0) == 0x80) j++;
        add_error(errs, max_errs, nerrs, line, col, "unexpected character", text + i, j - i,
              HINT_UNKNOWN);
        i = j;
    }

    push(toks, &n, g->end, END_MARKER, strlen(END_MARKER), line, (int)len + 1 + col_offset);
    return n;
}

void token_str(const Grammar *g, const Token *t, char *buf, size_t n)
{
    const char *kind = g->names[t->kind];
    if (strcmp(kind, t->lexeme) == 0) snprintf(buf, n, "%s", kind);
    else snprintf(buf, n, "%s(%s)", kind, t->lexeme);
}
