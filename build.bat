@echo off
REM Windows build (MinGW / TDM-GCC / MSYS2 gcc on PATH)
REM   build.bat        -> slr.exe and test_slr.exe, then runs the tests

set SRC=src\grammar.c src\lexer.c src\lr0.c src\first_follow.c src\slr_table.c src\sdt.c src\parser.c src\pipeline.c
set FLAGS=-std=c99 -Wall -Wextra -pedantic -O2 -Iinclude

gcc %FLAGS% %SRC% src\main.c -o slr.exe || goto :fail
gcc %FLAGS% %SRC% tests\test_slr.c -o test_slr.exe || goto :fail
echo Build OK.
test_slr.exe
goto :eof

:fail
echo Build FAILED.
exit /b 1
