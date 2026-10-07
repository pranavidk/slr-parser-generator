# SLR Parser Generator — Team 7

Design and Implementation of an SLR Parser Generator Using LR(0) Item Sets,
Closure-GOTO Construction, ACTION-GOTO Tables and Syntax-Directed Translation.

Team: 24BCE2760, 24BCE2773, 24BCE2778, 24BCE2819, 24BCE2860

Repository: <https://github.com/pranavidk/slr-parser-generator>

> Project history, design decisions, verification results and remaining goals
> are in **[PROJECT_NOTES.md](PROJECT_NOTES.md)**.

Plain **C99**, standard library only. Builds warning-free with
`clang -std=c99 -Wall -Wextra -pedantic`; no errors under AddressSanitizer
and UBSan, and no memory leaks.

## Build

Linux / macOS / WSL / MSYS2:
```bash
make            # builds ./slr
make test       # builds and runs the 35-test suite
make demo       # runs the demo commands
```

Windows (MinGW / TDM-GCC on PATH):
```bat
build.bat
```

Or by hand anywhere:
```bash
gcc -std=c99 -O2 -Iinclude src/*.c -o slr
```

Run from the project folder (the default grammar path is `grammars/assignment.grammar`).

## Run

```bash
./slr "x = a + b * c"                       # parse + TAC
./slr "x = a + b * c" --trace --quads       # shift-reduce trace + quadruples
./slr --all "y = (a + b) * c"               # every stage: grammar, states, FIRST/FOLLOW, table, trace
./slr --file examples/program.txt           # multi-statement program with errors (shows recovery)
./slr -g grammars/ambiguous.grammar         # conflict detection demo
./slr -g grammars/extended.grammar "r = a - b / c"   # different grammar, same code
```

Exit code: 0 = all OK, 1 = errors/conflicts reported, 2 = could not load input.

## Pipeline

```
grammar file -> Grammar (augment S'->S) -> LR(0) collection (closure/GOTO)
             -> FIRST/FOLLOW -> SLR ACTION/GOTO table (+ conflict report)
source text  -> Lexer -> shift-reduce driver (+ value stack) -> semantic actions -> TAC / quadruples
```

## Module ownership

| Member | Name | Files | Responsibility |
|---|---|---|---|
| 1 | Isaac (24BCE2760) | `grammar.c/.h`, `lexer.c/.h` | Grammar file parsing, numbering, augmentation, validation; tokenizer |
| 2 | Prithvi (24BCE2778) | `lr0.c/.h` | LR(0) items, `lr0_closure()`, `lr0_goto()`, canonical collection |
| 3 | Nitin (24BCE2819) | `first_follow.c/.h`, `slr_table.c/.h` | FIRST/FOLLOW (bitsets), ACTION/GOTO table, shift/reduce & reduce/reduce conflict detection |
| 4 | Shafin (24BCE2860) | `parser.c/.h`, `pipeline.c/.h` | Table-driven driver, parse trace, diagnostics, statement-level error recovery |
| 5 | Pranav (24BCE2773) | `sdt.c/.h` | Semantic actions, `place` attributes, `newTemp()`, TAC + quadruples |
| all | Whole team | `main.c`, `tests/test_slr.c` | CLI, verification suite |

## Key data structures

| Structure | Representation |
|---|---|
| Production | `{ head, rhs[MAX_RHS], len }` — symbols are integer ids |
| LR(0) item | `{ prod, dot }` |
| State | sorted array of items (`ItemSet`), compared with `memcmp` |
| Transitions | `trans[state][symbol]` → state or -1 |
| FIRST / FOLLOW | 64-bit bitsets (`SymSet`), one bit per symbol |
| ACTION / GOTO | `Action action[state][terminal]` (`kind`, `target`), `int go[state][nonterminal]` |
| Parser stacks | parallel state stack + value (`place`) stack |
| TAC | quadruples `(op, arg1, arg2, result)` |

Limits are `#define`s at the top of each header (e.g. `MAX_STATES 256`,
`MAX_SYMBOLS 64`, `MAX_STACK 512`); exceeding one gives a clear error, never a crash.

## Results on the project grammar

* 9 productions, **16 LR(0) states**, 0 conflicts → grammar is SLR(1)
* FOLLOW(E) = { +, ), $ }   FOLLOW(T) = FOLLOW(F) = { +, *, ), $ }
* `x = a + b * c` → `t1 = b * c`, `t2 = a + t1`, `x = t2` (slide 9)

## Review 2 rubric mapping

| Criterion (marks) | Where it is shown |
|---|---|
| Core algorithm completeness & correctness (6) | `./slr --all ...`: every stage from grammar to TAC; works on any grammar file |
| Verified against Review 1 test cases (4) | `make test` → section A, cases 1–7 from slide 10 |
| Error handling & diagnostics (3) | Line/col + caret, parser state, expected tokens, targeted hint; lexical errors; recovery continues to next statement; conflict report with items |
| Code quality (2) | One module per stage, header comments, 35 tests, warning-free, sanitizer-clean, leak-free |

## Error diagnostics

```
Syntax error at line 6, col 14: unexpected end of input
    bad2 = (a + b
                 ^
    parser state : I12
    expected     : '+', ')'
    hint         : Input ended while a '(' is still open - add the missing ')'.
```

Hints cover: missing operand, doubled operator, missing operator, unclosed `(`,
unmatched `)`, empty `()`, statements not starting with an identifier, unknown
characters, over-long identifiers and over-deep nesting.

An SLR parser can perform a few reductions before detecting an error (FOLLOW
sets are coarser than exact lookaheads) but never shifts a bad token, so the
reported position is always exact.

## Grammar file format

```
S -> id = E
E -> E + T | T
T -> T * F | F
F -> ( E ) | id | num
```

The first rule's head is the start symbol. Symbols that never appear on a
left-hand side are terminals. `epsilon` / `eps` marks an empty body. `#` starts
a comment.
