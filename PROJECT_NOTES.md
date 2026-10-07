# Project Notes — SLR Parser Generator (Team 7)

Running record of what has been built, the decisions behind it, how it was
verified, and what is left for the final stage. The [README](README.md) covers
how to build and run the code. This file covers the project history and the
reasons behind each decision.

---

## 1. Project at a glance

| | |
|---|---|
| **Title** | Design and Implementation of an SLR Parser Generator Using LR(0) Item Sets, Closure-GOTO Construction, ACTION-GOTO Tables and Syntax-Directed Translation |
| **Course** | Compiler Design — G1 slot, Team 7 |
| **Team** | 24BCE2760 (Isaac), 24BCE2773 (Pranav), 24BCE2778 (Prithvi), 24BCE2819 (Nitin Sunil), 24BCE2860 (Shafin) |
| **Final language** | C (C99, standard library only) |
| **Status** | Review 1 done · Review 2 implementation complete and verified · Final presentation pending |

### Assessment (50 marks total)

| Stage | Marks | Criteria |
|---|---|---|
| Review 1 — Design & Initial Implementation | 15 | Problem/scope/grammar 3 · Algorithm design 5 · Initial working component 4 · Test case design 2 · Team coordination 1 |
| **Review 2 — Core Implementation & Testing** | **15** | **Algorithm completeness & correctness 6 · Verified against Review 1 test cases 4 · Error handling & diagnostics 3 · Code quality 2** |
| Final Presentation + Project Document + Plagiarism Report | 20 | Report must cover: problem & scope, grammar, design, module-wise implementation, test cases & results, screenshots, conclusion, individual contributions |

---

## 2. What we've worked on (timeline)

### Review 1 (slides dated 1 Sep 2026)
- Problem statement and scope: Grammar → LR(0) states → SLR table → parsing → semantic actions → TAC.
- Fixed the project grammar (9 numbered productions after augmentation, see §3).
- Algorithm design for closure(), GOTO(), the canonical collection, the SLR table rules, the parser loop, conflict detection, error reporting and SDT action templates.
- **Working component:** the LR(0) item set generator. Sample run: 9 productions, **16 LR(0) states**.
- Test plan of 7 cases (2 valid, 2 edge, 3 invalid), slide 10.
- The code snippet on the slides was C++-style (`set<>`, `map<>`). The final code is plain C (see below).

### Prototype (early Sep 2026)
- A single-file Python prototype covered the full pipeline on the textbook grammar `E → E+T | T …`. It was used to understand the algorithm and is not part of the submission.

### Review 2 implementation, first version (6 Oct 2026)
- A modular Python implementation on the real project grammar, with one module per team member, TAC/quadruple output, diagnostics and statement-level error recovery. It passed 31 tests.

### Review 2 implementation, final version in C (7 Oct 2026)
- **C was confirmed as the preferred language**, so the whole implementation was ported to C99 with the same module split.
- The C output was checked against the Python version on every demo input and all results are identical: states, FIRST/FOLLOW, every table cell, every trace step, TAC, conflicts and every error message. The only differences are layout (a labelled ACTION/GOTO header row, and the ACCEPTED status printed after the trace).
- **The C version is the one to submit and present.** The Python version is only a reference.

---

## 3. Language & grammar

A small expression language with assignment, `+`, `*`, parentheses, identifiers and numeric literals.

```
0. S' -> S          (augmented)
1. S  -> id = E
2. E  -> E + T
3. E  -> T
4. T  -> T * F
5. T  -> F
6. F  -> ( E )
7. F  -> id
8. F  -> num
```

- Terminals: `id = + * ( ) num $` · Nonterminals: `S E T F` · Start: `S` (augmented `S'`)
- Precedence (`*` over `+`) and left-associativity come from the grammar's layering. No precedence declarations are needed.
- FOLLOW(S) = { $ } · FOLLOW(E) = { +, ), $ } · FOLLOW(T) = FOLLOW(F) = { +, *, ), $ }
- Result: **16 LR(0) states, 0 conflicts, so the grammar is SLR(1).**

Two extra grammars are included for the demo:
- `grammars/extended.grammar` adds `-` and `/` and produces 20 states with 0 conflicts. It shows that the generator works on any grammar file without code changes.
- `grammars/ambiguous.grammar` is `E → E+E | E*E | …`, which gives 4 shift/reduce conflicts. It demonstrates conflict detection.

---

## 4. Final implementation

### Pipeline

```
grammar file ──► Grammar ──► LR(0) collection ──► FIRST/FOLLOW ──► SLR ACTION/GOTO table
                (augment)    (closure/GOTO)                         (+ conflict report)
                                                                          │
source text ──► Lexer ──► shift-reduce driver + value stack ──► semantic actions ──► TAC / quadruples
```

### Modules

| Member | Name | Files | What it does |
|---|---|---|---|
| 1 | Isaac (24BCE2760) | `grammar.c/.h`, `lexer.c/.h` | Reads the grammar text, numbers the productions, classifies symbols, adds `S' → S`, and warns about unreachable or non-productive nonterminals. The lexer derives its token classes from the grammar's terminals. |
| 2 | Prithvi (24BCE2778) | `lr0.c/.h` | LR(0) items `(prod, dot)`, `lr0_closure()`, `lr0_goto()`, and the canonical collection built by fixed-point iteration with stable state numbering. |
| 3 | Nitin (24BCE2819) | `first_follow.c/.h`, `slr_table.c/.h` | FIRST/FOLLOW as 64-bit bitsets with ε support. The ACTION/GOTO table uses the slide 7 rules. Shift/reduce and reduce/reduce conflicts are recorded with the state, terminal and items involved. |
| 4 | Shafin (24BCE2860) | `parser.c/.h`, `pipeline.c/.h` | The table-driven driver with parallel state and value stacks, the step-by-step trace, diagnostics, and statement-level error recovery. |
| 5 | Pranav (24BCE2773) | `sdt.c/.h` | The `place` attribute, `newTemp()`, the semantic actions from slide 9, and TAC plus quadruples. |
| all | Whole team | `main.c`, `tests/test_slr.c` | The command-line driver and the 35-test verification suite. |

The Review 1 slides do not list module owners. The assignment above was recorded on 7 Oct 2026.

### Key data structures

| Concept | C representation |
|---|---|
| Symbol | integer id plus a name table; `is_terminal[]` flag |
| Production | `struct { int head; int rhs[MAX_RHS]; int len; }` |
| LR(0) item | `struct { int prod; int dot; }` |
| State | sorted `Item` array (`ItemSet`), compared with `memcmp` |
| Transitions | `int trans[MAX_STATES][MAX_SYMBOLS]` (−1 = none) |
| FIRST / FOLLOW | `unsigned long long` bitsets, one bit per symbol |
| ACTION / GOTO | `Action action[state][terminal]` (`kind`, `target`) · `int go[state][nonterminal]` |
| Parser | parallel state stack + value (`place`) stack |
| TAC | quadruples `(op, arg1, arg2, result)` |

### Design decisions (and why)

1. **The grammar is read from a file, not hard-coded.** This makes it a true parser *generator*: changing the language means editing a text file.
2. **Semantic actions are chosen by the production's shape** (`A → X`, `S → id = E`, `F → ( E )`, `A → A op B`) rather than by production number. Reordering the grammar or adding `-` and `/` therefore needs no code change.
3. **Conflicts are reported, then resolved yacc-style:** shift wins over reduce, and on reduce/reduce the lower-numbered production wins. This keeps the table usable while still reporting that the grammar is not SLR(1).
4. **Error recovery is statement-level panic mode.** After an error the rest of that statement is skipped, along with any TAC it had generated, and parsing resumes at the next statement. One run reports every faulty statement.
5. **Diagnostics are aimed at students.** They show line and column with a caret, the parser state, the expected terminals (the non-empty ACTION cells) and a targeted hint (missing operand, doubled operator, missing operator, unclosed `(`, unmatched `)`, empty `()`, bad first token).
6. **Output is plain ASCII** (`.` for the item dot, `eps` for ε) so it displays correctly in the Windows command prompt.
7. **Limits are fixed and checked.** They are `#define`s at the top of each header (`MAX_STATES 256`, `MAX_SYMBOLS 64`, `MAX_STACK 512`, …). Going over one gives a clear error message, never a crash.

---

## 5. Verification

### Review 1 test plan — actual results

| # | Category | Input | Result | Output |
|---|---|---|---|---|
| 1 | Valid | `x = 5 + y * 2` | Accepted | `t1 = y * 2` · `t2 = 5 + t1` · `x = t2` |
| 2 | Valid | `x = (y + 5) * 2` | Accepted | `t1 = y + 5` · `t2 = t1 * 2` · `x = t2` |
| 3 | Edge | `x = 7` | Accepted | `x = 7` |
| 4 | Edge | `x = (((7)))` | Accepted | `x = 7` |
| 5 | Invalid | `x = + 7` | Rejected | col 5, unexpected `'+'`, state I3, expected `id ( num` |
| 6 | Invalid | `x = (7 + y` | Rejected | col 11, unexpected end of input, state I12, expected `+ )`, hint: missing `')'` |
| 7 | Invalid | `x = 7 *` | Rejected | col 8, unexpected end of input, state I11, expected `id ( num`, hint: operand missing |

The slide 9 example `x = a + b * c` gives `t1 = b * c` · `t2 = a + t1` · `x = t2`, which matches the slide.

### Automated checks
- **35 tests** (`make test`), grouped by module: Review 1 cases (7), grammar (4), LR(0) (3), FIRST/FOLLOW/table/conflicts (5), parser and recovery (8), SDT/TAC (6), plus 2 table-driven end-to-end tests covering 13 accepted and 7 rejected programs across all three grammars.
- The build has **zero warnings** under Apple clang 21 with `-std=c99 -Wall -Wextra -pedantic`, at both `-O0` and `-O2` (checked 7 Oct 2026).
- AddressSanitizer and UBSan report **no memory errors** on the test suite and on `--all` runs of all three grammars. The macOS `leaks` tool reports **0 leaks** for the test suite and the demo runs (checked 7 Oct 2026).
- Not yet checked: a real `gcc` build and valgrind. Both need Linux, WSL or MinGW (see §8).
- The C output was cross-checked against the independent Python implementation on all demo inputs and is identical.

---

## 6. Review 2 demo plan (rubric order)

| Rubric item | Command | What to point at |
|---|---|---|
| Completeness & correctness (6) | `./slr --all "x = a + b * c"` | Every stage in order: grammar → 16 states → FIRST/FOLLOW → table → trace → TAC |
| | `./slr -g grammars/extended.grammar "r = a - b / c - d"` | A different grammar works with no code change |
| Verified against Review 1 tests (4) | `make test` | Section A: the 7 slide-10 cases all pass |
| Error handling & diagnostics (3) | `./slr --file examples/program.txt` | 4 different errors in one run, carets, hints, recovery continues |
| | `./slr -g grammars/ambiguous.grammar` | Conflict report naming states, terminals and items |
| Code quality (2) | Show `include/` headers and the module table | One file per stage, documented headers, test suite, warning-free build |

---

## 7. Known limitations

- An SLR parser can perform a few extra reductions before it detects an error, because FOLLOW sets are coarser than exact lookaheads. It never shifts a bad token, so the reported error position is always exact.
- Recovery is per statement. Within one statement, only the first syntax error is reported.
- For a grammar that is not SLR(1), the shift-wins resolution produces a usable table but not necessarily the intended meaning. With `ambiguous.grammar`, `x = a * b + c` translates as `a * (b + c)` (`t1 = b + c`, `t2 = a * t1`), because every operator binds tighter than the ones before it. The conflict report is the warning. The fix is to layer the grammar, as `assignment.grammar` does.
- Semantic actions cover the shapes our language uses (copy, assignment, parentheses, binary operators). A grammar with other shapes, such as unary minus or function calls, gets a clear "no semantic action defined" error and would need a new rule in `sdt.c`.
- There is no type checking or symbol table. That is outside this project's scope (it is Team 11's title).

---

## 8. Remaining goals — Final Presentation (20 marks)

- [ ] **Project document** following the report template:
  - [ ] Problem statement & scope (§1, §3)
  - [ ] Grammar / language specification (§3)
  - [ ] Design: algorithm chosen and why it fits SLR (§4 pipeline + design decisions)
  - [ ] Implementation details, module-wise, mapped to team members (§4 module table)
  - [ ] Test cases and results (§5)
  - [ ] Screenshots / output samples: the `--all` run, the error run, the conflict run and `make test`
  - [ ] Conclusion and **individual contribution summary** (each member describes their module)
- [ ] **Plagiarism report** for the document
- [ ] **Final presentation slides**, updated from Review 1 with real outputs replacing the illustrative ones (e.g. the slide 7 table fragment shows illustrative state numbers; use the real table from `--table`)
- [ ] Commit everything to the team repository, so commit history shows each member's work (Review 1 asked for version control as evidence of team coordination)
- [ ] Build with real `gcc` (Linux/WSL, or `build.bat` under MinGW) and run `valgrind ./test_slr` to confirm the warning-free and leak-free claims there too
- [ ] Optional extras if time allows: print the parse tree, add unary minus, export the table as CSV

---

## 9. Folder layout

```
slr_c/
├── README.md            build & run instructions
├── PROJECT_NOTES.md     this file
├── Makefile             make / make test / make demo
├── build.bat            Windows build
├── include/             one header per module (documented)
├── src/                 grammar, lexer, lr0, first_follow, slr_table, parser, sdt, pipeline, main
├── tests/test_slr.c     35-test verification suite
├── grammars/            assignment (project), extended, ambiguous
└── examples/program.txt multi-statement demo with deliberate errors
```
