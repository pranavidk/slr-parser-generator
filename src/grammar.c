/*
 * Module 1 - Grammar Specification, Loading & Augmentation (Owner: Member 1)
 */
#include "grammar.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char head[MAX_NAME];
    char body[MAX_RHS][MAX_NAME];
    int  len;
} RawRule;

/* ------------------------------------------------------------------ */
/* Utilities                                                           */
/* ------------------------------------------------------------------ */
void set_error(char *err, size_t errlen, const char *fmt, ...)
{
    va_list ap;
    if (!err || errlen == 0) return;
    va_start(ap, fmt);
    vsnprintf(err, errlen, fmt, ap);
    va_end(ap);
}

void str_append(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap;
    size_t used = strlen(buf);
    if (used + 1 >= n) return;
    va_start(ap, fmt);
    vsnprintf(buf + used, n - used, fmt, ap);
    va_end(ap);
}

char *read_text_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    long size;
    size_t got;

    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';

    /* Strip a UTF-8 byte-order mark (Windows Notepad adds one) */
    if (got >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF)
        memmove(buf, buf + 3, got - 2);
    return buf;
}

static char *trim(char *s)
{
    char *e;
    while (isspace((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

/* Split s in place on whitespace. Returns count, or -1 if more than max. */
static int split_ws(char *s, char **out, int max)
{
    int n = 0;
    while (*s) {
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) break;
        if (n == max) return -1;
        out[n++] = s;
        while (*s && !isspace((unsigned char)*s)) s++;
        if (*s) *s++ = '\0';
    }
    return n;
}

/* UTF-8 arrow (E2 86 92) -> "-> " (same byte length) */
static void replace_arrow(char *s)
{
    char *p;
    while ((p = strstr(s, "\xE2\x86\x92")) != NULL) {
        p[0] = '-';
        p[1] = '>';
        p[2] = ' ';
    }
}

static int is_epsilon(const char *s)
{
    return strcmp(s, "\xCE\xB5") == 0 || strcmp(s, "epsilon") == 0 || strcmp(s, "eps") == 0;
}

/* ------------------------------------------------------------------ */
/* Queries                                                             */
/* ------------------------------------------------------------------ */
int grammar_find(const Grammar *g, const char *name)
{
    int i;
    for (i = 0; i < g->nsyms; i++)
        if (strcmp(g->names[i], name) == 0) return i;
    return -1;
}

void production_str(const Grammar *g, int prod, char *buf, size_t n)
{
    const Production *p = &g->prods[prod];
    int k;
    snprintf(buf, n, "%s ->", g->names[p->head]);
    for (k = 0; k < p->len; k++) str_append(buf, n, " %s", g->names[p->rhs[k]]);
    if (p->len == 0) str_append(buf, n, " %s", EPSILON_TEXT);
}

/* ------------------------------------------------------------------ */
/* Construction                                                        */
/* ------------------------------------------------------------------ */
static int name_used(const RawRule *raw, int nraw, const char *name)
{
    int i, k;
    for (i = 0; i < nraw; i++) {
        if (strcmp(raw[i].head, name) == 0) return 1;
        for (k = 0; k < raw[i].len; k++)
            if (strcmp(raw[i].body[k], name) == 0) return 1;
    }
    return 0;
}

static int is_head(const RawRule *raw, int nraw, const char *name)
{
    int i;
    for (i = 0; i < nraw; i++)
        if (strcmp(raw[i].head, name) == 0) return 1;
    return 0;
}

static int intern(Grammar *g, const char *name, int terminal)
{
    int id = grammar_find(g, name);
    if (id >= 0) return id;
    if (g->nsyms >= MAX_SYMBOLS) return -1;
    id = g->nsyms++;
    strcpy(g->names[id], name);
    g->is_terminal[id] = terminal;
    if (terminal) g->terms[g->nterms++] = id;
    else g->nonterms[g->nnonterms++] = id;
    return id;
}

static void add_warning(Grammar *g, const char *fmt, const char *a, const char *b)
{
    char ca[MAX_NAME], cb[MAX_NAME];   /* copies: a and b point into *g */
    if (g->nwarnings >= MAX_WARNINGS) return;
    snprintf(ca, sizeof ca, "%s", a);
    snprintf(cb, sizeof cb, "%s", b);
    snprintf(g->warnings[g->nwarnings++], sizeof g->warnings[0], fmt, ca, cb);
}

static void validate(Grammar *g)
{
    int reach[MAX_SYMBOLS] = {0}, productive[MAX_SYMBOLS] = {0};
    int stack[MAX_SYMBOLS], sp = 0, i, k, changed;

    /* Reachability from S' */
    reach[g->aug_start] = 1;
    stack[sp++] = g->aug_start;
    while (sp > 0) {
        int nt = stack[--sp];
        for (i = 0; i < g->nprods; i++) {
            if (g->prods[i].head != nt) continue;
            for (k = 0; k < g->prods[i].len; k++) {
                int s = g->prods[i].rhs[k];
                if (!g->is_terminal[s] && !reach[s]) {
                    reach[s] = 1;
                    stack[sp++] = s;
                }
            }
        }
    }
    for (i = 0; i < g->nnonterms; i++)
        if (!reach[g->nonterms[i]])
            add_warning(g, "Nonterminal '%s' is unreachable from start symbol '%s'.",
                        g->names[g->nonterms[i]], g->names[g->start]);

    /* Productivity: can derive some terminal string */
    for (i = 0; i < g->nterms; i++) productive[g->terms[i]] = 1;
    do {
        changed = 0;
        for (i = 0; i < g->nprods; i++) {
            const Production *p = &g->prods[i];
            int all = 1;
            if (productive[p->head]) continue;
            for (k = 0; k < p->len; k++)
                if (!productive[p->rhs[k]]) all = 0;
            if (all) {
                productive[p->head] = 1;
                changed = 1;
            }
        }
    } while (changed);
    for (i = 0; i < g->nnonterms; i++)
        if (!productive[g->nonterms[i]])
            add_warning(g, "Nonterminal '%s' can never derive a terminal string.%s",
                        g->names[g->nonterms[i]], "");
}

static int build(Grammar *g, const RawRule *raw, int nraw, char *err, size_t errlen)
{
    char aug[MAX_NAME];
    int i, k;

    /* S' (add more primes if the name is already taken) */
    strcpy(aug, raw[0].head);
    do {
        if (strlen(aug) + 1 >= MAX_NAME) {
            set_error(err, errlen, "Start symbol name is too long to augment.");
            return -1;
        }
        strcat(aug, "'");
    } while (name_used(raw, nraw, aug));

    g->aug_start = intern(g, aug, 0);
    for (i = 0; i < nraw; i++)
        if (intern(g, raw[i].head, 0) < 0) goto too_many;
    for (i = 0; i < nraw; i++) {
        for (k = 0; k < raw[i].len; k++) {
            const char *s = raw[i].body[k];
            if (is_head(raw, nraw, s)) continue;
            if (strcmp(s, END_MARKER) == 0) {
                set_error(err, errlen,
                          "'%s' is reserved for end-of-input and cannot be used in rules.",
                          END_MARKER);
                return -1;
            }
            if (intern(g, s, 1) < 0) goto too_many;
        }
    }
    if ((g->end = intern(g, END_MARKER, 1)) < 0) goto too_many;
    g->start = grammar_find(g, raw[0].head);

    g->prods[0].head = g->aug_start;
    g->prods[0].rhs[0] = g->start;
    g->prods[0].len = 1;
    for (i = 0; i < nraw; i++) {
        Production *p = &g->prods[i + 1];
        p->head = grammar_find(g, raw[i].head);
        p->len = raw[i].len;
        for (k = 0; k < p->len; k++) p->rhs[k] = grammar_find(g, raw[i].body[k]);
    }
    g->nprods = nraw + 1;
    validate(g);
    return 0;

too_many:
    set_error(err, errlen, "Grammar uses more than %d symbols.", MAX_SYMBOLS);
    return -1;
}

int grammar_from_text(Grammar *g, const char *text, char *err, size_t errlen)
{
    RawRule *raw = calloc(MAX_PRODS, sizeof *raw);
    char line[1024];
    const char *p = text;
    int nraw = 0, lineno = 0, rc = -1;

    memset(g, 0, sizeof *g);
    if (!raw) {
        set_error(err, errlen, "Out of memory.");
        return -1;
    }
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;

    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char *s, *arrow, *head, *alt, *heads[2];
        int nh;

        lineno++;
        if (len >= sizeof line) {
            set_error(err, errlen, "Line %d: line is too long.", lineno);
            goto done;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        p += len + (nl ? 1 : 0);

        if ((s = strchr(line, '#')) != NULL) *s = '\0';
        replace_arrow(line);
        s = trim(line);
        if (!*s) continue;

        arrow = strstr(s, "->");
        if (!arrow) {
            set_error(err, errlen, "Line %d: missing '->' in rule: '%s'", lineno, s);
            goto done;
        }
        *arrow = '\0';
        head = trim(s);
        nh = split_ws(head, heads, 2);
        if (nh != 1) {
            set_error(err, errlen, "Line %d: rule head must be a single symbol.", lineno);
            goto done;
        }
        if (strlen(heads[0]) >= MAX_NAME) {
            set_error(err, errlen, "Line %d: symbol name '%s' is too long (max %d chars).",
                      lineno, heads[0], MAX_NAME - 1);
            goto done;
        }

        alt = arrow + 2;
        for (;;) {
            char *bar = strchr(alt, '|');
            char *syms[MAX_RHS + 1];
            int n, k;
            RawRule *r;

            if (bar) *bar = '\0';
            n = split_ws(alt, syms, MAX_RHS + 1);
            if (n < 0 || n > MAX_RHS) {
                set_error(err, errlen, "Line %d: production body longer than %d symbols.",
                          lineno, MAX_RHS);
                goto done;
            }
            if (n == 0) {
                set_error(err, errlen,
                          "Line %d: empty alternative (write 'epsilon' for an empty body).",
                          lineno);
                goto done;
            }
            if (nraw >= MAX_PRODS - 1) {
                set_error(err, errlen, "Too many productions (max %d).", MAX_PRODS - 1);
                goto done;
            }
            r = &raw[nraw++];
            strcpy(r->head, heads[0]);
            if (n == 1 && is_epsilon(syms[0])) {
                r->len = 0;
            } else {
                for (k = 0; k < n; k++) {
                    if (strlen(syms[k]) >= MAX_NAME) {
                        set_error(err, errlen, "Line %d: symbol name '%s' is too long.",
                                  lineno, syms[k]);
                        goto done;
                    }
                    strcpy(r->body[k], syms[k]);
                }
                r->len = n;
            }
            if (!bar) break;
            alt = bar + 1;
        }
    }

    if (nraw == 0) {
        set_error(err, errlen, "Grammar text contains no rules.");
        goto done;
    }
    rc = build(g, raw, nraw, err, errlen);

done:
    free(raw);
    return rc;
}

int grammar_from_file(Grammar *g, const char *path, char *err, size_t errlen)
{
    char *text = read_text_file(path);
    int rc;
    if (!text) {
        set_error(err, errlen, "Cannot read grammar file '%s'.", path);
        return -1;
    }
    rc = grammar_from_text(g, text, err, errlen);
    free(text);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Display                                                             */
/* ------------------------------------------------------------------ */
void grammar_print(const Grammar *g, FILE *out)
{
    char buf[512];
    int i;

    fprintf(out, "Augmented grammar (numbered productions):\n");
    for (i = 0; i < g->nprods; i++) {
        production_str(g, i, buf, sizeof buf);
        fprintf(out, "  %d. %s\n", i, buf);
    }
    fprintf(out, "\nTerminals    : ");
    for (i = 0; i < g->nterms; i++)
        fprintf(out, "%s%s", i ? ", " : "", g->names[g->terms[i]]);
    fprintf(out, "\nNonterminals : ");
    for (i = 1; i < g->nnonterms; i++)
        fprintf(out, "%s%s", i > 1 ? ", " : "", g->names[g->nonterms[i]]);
    fprintf(out, "\nStart symbol : %s (augmented %s)\n",
            g->names[g->start], g->names[g->aug_start]);
    for (i = 0; i < g->nwarnings; i++) fprintf(out, "WARNING: %s\n", g->warnings[i]);
}
