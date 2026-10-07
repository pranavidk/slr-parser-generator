/*
 * SLR Parser Generator - command-line driver.
 *
 *   slr "x = a + b * c"
 *   slr "x = a + b * c" --trace
 *   slr --file examples/program.txt
 *   slr --all "y = (a + b) * c"
 *   slr --grammar grammars/ambiguous.grammar --table
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline.h"

#define DEFAULT_GRAMMAR "grammars/assignment.grammar"

static void banner(const char *title)
{
    printf("\n======================================================================\n"
           " %s\n"
           "======================================================================\n",
           title);
}

static void usage(const char *prog)
{
    printf("Usage: %s [options] [\"statement; statement ...\"]\n\n"
           "Options:\n"
           "  -f, --file FILE      read the program from FILE\n"
           "  -g, --grammar FILE   grammar file (default %s)\n"
           "      --grammar-info   print the augmented grammar\n"
           "      --states         print the LR(0) canonical collection\n"
           "      --sets           print FIRST and FOLLOW sets\n"
           "      --table          print the ACTION/GOTO table\n"
           "      --trace          print the shift-reduce trace\n"
           "      --quads          print quadruples as well as TAC\n"
           "      --all            print every stage\n"
           "  -h, --help           show this help\n",
           prog, DEFAULT_GRAMMAR);
}

static const char *base_name(const char *path)
{
    const char *a = strrchr(path, '/'), *b = strrchr(path, '\\');
    const char *s = a > b ? a : b;
    return s ? s + 1 : path;
}

int main(int argc, char **argv)
{
    const char *grammar_path = DEFAULT_GRAMMAR, *file = NULL, *inline_src = NULL;
    int show_grammar = 0, show_states = 0, show_sets = 0, show_table = 0;
    int show_trace = 0, show_quads = 0, all = 0;
    char err[512], line_buf[MAX_LINE], qbuf[256];
    char *source = NULL;
    SLRGen *gen;
    Statement *stmts;
    StatementResult *res;
    int i, k, n, accepted = 0, errors = 0;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(argv[0]); return 0; }
        else if ((!strcmp(a, "-g") || !strcmp(a, "--grammar")) && i + 1 < argc) grammar_path = argv[++i];
        else if ((!strcmp(a, "-f") || !strcmp(a, "--file")) && i + 1 < argc) file = argv[++i];
        else if (!strcmp(a, "--grammar-info")) show_grammar = 1;
        else if (!strcmp(a, "--states")) show_states = 1;
        else if (!strcmp(a, "--sets")) show_sets = 1;
        else if (!strcmp(a, "--table")) show_table = 1;
        else if (!strcmp(a, "--trace")) show_trace = 1;
        else if (!strcmp(a, "--quads")) show_quads = 1;
        else if (!strcmp(a, "--all")) all = 1;
        else if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "Unknown or incomplete option '%s' (try --help)\n", a);
            return 2;
        } else if (!inline_src) inline_src = a;
        else {
            fprintf(stderr, "Only one source string is allowed; separate statements with ';'.\n");
            return 2;
        }
    }
    if (all) show_grammar = show_states = show_sets = show_table = show_trace = show_quads = 1;

    gen = gen_from_file(grammar_path, err, sizeof err);
    if (!gen) {
        fprintf(stderr, "Cannot load grammar: %s\n", err);
        return 2;
    }

    printf("Grammar: %s | productions: %d | LR(0) states: %d | conflicts: %d | %s\n",
           base_name(grammar_path), gen->g.nprods, gen->lr.nstates, gen->table.nconflicts,
           table_is_slr(&gen->table) ? "SLR(1)" : "NOT SLR(1)");

    if (show_grammar) { banner("1. Augmented grammar"); grammar_print(&gen->g, stdout); }
    if (show_states) { banner("2. LR(0) canonical collection (closure / GOTO)"); lr0_print(&gen->lr, stdout); }
    if (show_sets) { banner("3. FIRST / FOLLOW sets"); sets_print(&gen->g, &gen->sets, stdout); }
    if (show_table) { banner("4. SLR ACTION / GOTO table"); table_print(&gen->table, stdout); }
    if (show_table || !table_is_slr(&gen->table)) {
        printf("\n");
        table_print_conflicts(&gen->table, stdout);
    }

    if (file) {
        source = read_text_file(file);
        if (!source) {
            fprintf(stderr, "Cannot read program file '%s'.\n", file);
            gen_free(gen);
            return 2;
        }
    } else if (inline_src) {
        size_t len = strlen(inline_src);
        source = malloc(len + 1);
        if (!source) { gen_free(gen); return 2; }
        memcpy(source, inline_src, len + 1);
    } else {
        int rc = table_is_slr(&gen->table) ? 0 : 1;
        gen_free(gen);
        return rc;
    }

    n = gen_split(gen, source, NULL, 0);
    stmts = malloc((size_t)(n ? n : 1) * sizeof *stmts);
    res = malloc(sizeof *res);
    if (!stmts || !res) {
        fprintf(stderr, "Out of memory.\n");
        free(stmts); free(res); free(source); gen_free(gen);
        return 2;
    }
    n = gen_split(gen, source, stmts, n);
    tac_reset(&gen->tac);

    banner("5. Parsing + syntax-directed translation");
    for (i = 0; i < n; i++) {
        if (show_trace) {
            printf("\n[line %d] %s\n", stmts[i].line, stmts[i].text);
            gen_compile_statement(gen, &stmts[i], stdout, res);
            printf("  => %s\n", res->accepted ? "ACCEPTED" : "REJECTED");
        } else {
            gen_compile_statement(gen, &stmts[i], NULL, res);
            printf("\n[line %d] %s  ->  %s\n", res->line, res->source,
                   res->accepted ? "ACCEPTED" : "REJECTED");
        }
        for (k = res->quad_start; k < res->quad_end; k++) {
            quad_str(&gen->tac.quads[k], qbuf, sizeof qbuf);
            printf("    %s\n", qbuf);
        }
        for (k = 0; k < res->ndiags; k++) {
            const Diagnostic *d = &res->diags[k];
            int have = get_source_line(source, d->line, line_buf, sizeof line_buf) == 0;
            diag_print(&gen->g, d, have ? line_buf : NULL, stdout);
        }
        accepted += res->accepted;
        errors += res->ndiags;
    }

    banner("Three-address code (accepted statements)");
    if (gen->tac.nquads == 0) printf("(none)\n");
    for (k = 0; k < gen->tac.nquads; k++) {
        quad_str(&gen->tac.quads[k], qbuf, sizeof qbuf);
        printf("%s\n", qbuf);
    }
    if (show_quads && gen->tac.nquads > 0) {
        printf("\n");
        tac_print_quads(&gen->tac, stdout);
    }
    printf("\nSummary: %d/%d statements accepted, %d error(s) reported.\n", accepted, n, errors);

    free(stmts);
    free(res);
    free(source);
    gen_free(gen);
    return errors ? 1 : 0;
}
