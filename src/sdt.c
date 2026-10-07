/*
 * Module 5 - SDT & Three-Address Code (Owner: Member 5)
 */
#include "sdt.h"

#include <string.h>

void tac_reset(TACGen *tg)
{
    tg->nquads = 0;
    tg->temp_count = 0;
}

static void copy_place(char *dst, const char *src)
{
    strncpy(dst, src, MAX_PLACE - 1);
    dst[MAX_PLACE - 1] = '\0';
}

static void new_temp(TACGen *tg, char *out)
{
    snprintf(out, MAX_PLACE, "t%d", ++tg->temp_count);
}

static int emit(TACGen *tg, const char *op, const char *a1, const char *a2, const char *res)
{
    Quad *q;
    if (tg->nquads >= MAX_QUADS) return -1;
    q = &tg->quads[tg->nquads++];
    snprintf(q->op, sizeof q->op, "%s", op);
    copy_place(q->arg1, a1);
    copy_place(q->arg2, a2);
    copy_place(q->result, res);
    return 0;
}

static int is_one_of(const char *s, const char *const *list)
{
    for (; *list; list++)
        if (strcmp(s, *list) == 0) return 1;
    return 0;
}

int tac_on_reduce(TACGen *tg, const Grammar *g, int prod,
                  char (*children)[MAX_PLACE], char *out, char *err, size_t errlen)
{
    static const char *const binary_ops[] = {"+", "-", "*", "/", "%", NULL};
    static const char *const assign_ops[] = {"=", ":=", NULL};
    const Production *p = &g->prods[prod];
    const char *b0 = p->len > 0 ? g->names[p->rhs[0]] : "";
    const char *b1 = p->len > 1 ? g->names[p->rhs[1]] : "";
    const char *b2 = p->len > 2 ? g->names[p->rhs[2]] : "";
    char pbuf[256];

    if (p->len == 0) {                                       /* A -> eps */
        out[0] = '\0';
        return 0;
    }
    if (p->len == 1) {                                       /* A -> X (copy rule) */
        copy_place(out, children[0]);
        return 0;
    }
    if (p->len == 3 && is_one_of(b1, assign_ops)) {          /* S -> id = E */
        if (emit(tg, "=", children[2], "", children[0]) < 0) goto full;
        copy_place(out, children[0]);
        return 0;
    }
    if (p->len == 3 && strcmp(b0, "(") == 0 && strcmp(b2, ")") == 0) {   /* F -> ( E ) */
        copy_place(out, children[1]);
        return 0;
    }
    if (p->len == 3 && is_one_of(b1, binary_ops)) {          /* A -> A1 op B */
        char t[MAX_PLACE];
        new_temp(tg, t);
        if (emit(tg, b1, children[0], children[2], t) < 0) goto full;
        copy_place(out, t);
        return 0;
    }

    production_str(g, prod, pbuf, sizeof pbuf);
    set_error(err, errlen, "no semantic action defined for production '%s'", pbuf);
    return -1;

full:
    set_error(err, errlen, "too many TAC instructions (max %d)", MAX_QUADS);
    return -1;
}

void quad_str(const Quad *q, char *buf, size_t n)
{
    if (strcmp(q->op, "=") == 0) snprintf(buf, n, "%s = %s", q->result, q->arg1);
    else snprintf(buf, n, "%s = %s %s %s", q->result, q->arg1, q->op, q->arg2);
}

void tac_print_quads(const TACGen *tg, FILE *out)
{
    int i;
    fprintf(out, "%-4s%-5s%-8s%-8s%s\n", "#", "op", "arg1", "arg2", "result");
    for (i = 0; i < tg->nquads; i++) {
        const Quad *q = &tg->quads[i];
        fprintf(out, "%-4d%-5s%-8s%-8s%s\n", i, q->op, q->arg1, q->arg2, q->result);
    }
}
