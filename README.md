# C-Shell

A custom POSIX-compliant interactive shell written in C. Every subsystem is isolated into its own source file. The binary is built with `make all` and requires no external libraries beyond the standard C POSIX interface.

---

## Building and Running

```
cd C-Shell
make all
./shell.out
```

`make clean` removes all build artifacts. The compiler flags enforced are:

```
-std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
-Wall -Wextra -Werror -Wno-unused-parameter -fno-asm
```

---

## Prompt

```
<username@hostname:cwd>
```

The directory where the shell was launched is treated as home. Any path that descends from it is displayed with the leading portion replaced by `~`. This substitution is recalculated on every prompt using `getcwd()` rather than relying on a cached value, so it stays correct after `chdir` calls from any source.

---

## Input

Lines are read with `fgets`. Trailing newlines and carriage returns are stripped. Blank or whitespace-only input is silently ignored. `Ctrl-D` exits cleanly. Signal interrupts (`EINTR`) cause the prompt to redisplay without printing an error.

---

## Parsing Pipeline

Input goes through three stages before anything is executed:

1. **Lexer** — character-level scan that splits the line into a `TokenList` (a null-terminated `char **` with a count). Operators (`|`, `;`, `&`, `<`, `>`, `>>`) are recognised without needing surrounding spaces. `>>` is identified as a two-character token during the scan so the parser never has to peek ahead for it.
2. **Validator** — single-pass check over the flat token array before any allocation. Rejects empty pipes, consecutive separators, dangling operators, and missing redirect targets. Prints `Invalid Syntax!` and stops if anything is wrong.
3. **Parser** — builds the Abstract Syntax Tree (AST). The tree has three levels:
   - `Command` — argv array, argc, optional input/output file, append flag.
   - `Pipeline` — linked list of `Command` nodes connected by `|`, plus a count.
   - `CommandGroup` — a `Pipeline` with an `is_background` flag, linked to the next group.

   Groups are separated by `;` or `&`. The parser walks the token array twice per command: once to count argv slots, once to populate them and extract redirections. All AST nodes are heap-allocated with `calloc` (zeroing all fields by default). Deallocation is recursive — `free_command_group_list` calls `free_pipeline` which calls `free_command`, each freeing only the node passed without touching `next`, so partial lists can be safely cleaned up on allocation failure mid-parse.

---

## Execution

### Foreground

Each foreground command runs in its own process group (`setpgid`). The shell hands terminal control to that group with `tcsetpgrp`, then waits using `waitpid(..., WUNTRACED)`. Terminal control returns to the shell when the process exits or stops.

### Pipelines

One child is forked per command. Between each pair of adjacent commands a `pipe()` is created. The read-end (`prev_fd`) is passed across iterations and wired to the next child's stdin via `dup2`. All children are collected and waited on together after all forks complete. File redirections on any individual command override the pipe endpoints, since `apply_redirections()` is called after the pipe `dup2`s inside the child.

### Background (`&`)

Background commands run in their own process group with stdin redirected to `/dev/null`. The shell does not wait for them. The job is registered in the in-memory job table and `[N] pid` is printed.

---

## Built-in Commands

Built-ins run in the shell process so they can affect shell state (working directory, job table, history file).

**hop** — `cd` equivalent. Accepts `~`, `.`, `..`, `-` (previous directory), or any path. Processes multiple arguments left to right.

**reveal** — directory listing. Flags `-a` (show hidden) and `-l` (long format with permissions, owner, size, mtime) may be combined. Entries are collected with `readdir`, sorted with `qsort` using `strcmp`, then printed. Accepts the same special path tokens as `hop`.

**log** — persistent command history backed by `log.txt`, capped at 15 entries. Commands containing the word `log` are excluded. Consecutive duplicates are not stored. When full, the oldest entry is dropped. Subcommands: `log` (print oldest-first), `log purge` (clear), `log execute N` (re-run the Nth most recent).

**activities** — prints all running and stopped background jobs sorted alphabetically by command name.

**ping `<pid>` `<signal>`** — sends a signal to a process. The signal number is taken modulo 32 before sending.

**fg `[N]`** — brings job N to the foreground, hands it terminal control, sends `SIGCONT`, and waits with `WUNTRACED`. If it stops again, it stays in the job table as stopped.

**bg `[N]`** — resumes a stopped job in the background by sending `SIGCONT` without giving it the terminal.

**exit** — exits the shell.

---

## Job Control

Before every prompt, `check_bg_jobs()` calls `waitpid(..., WNOHANG | WUNTRACED)` on each tracked job. Finished jobs are removed from the table with a `Done` line; stopped jobs are marked `Stopped` and stay in the table. `SIGINT` and `SIGTSTP` are ignored by the shell process — the kernel forwards them to the foreground process group, so the shell itself is never affected. `SIGTTOU` is ignored so background processes that accidentally write to the terminal do not stall.

---

## Design Choices

**Validate before allocating.** The validator runs a single pass over the flat token array and rejects bad input before any AST node is allocated. This keeps the parser simple — it never has to handle malformed input — and eliminates a category of partial-allocation cleanup bugs.

**AST recursive free.** Each `free_*` function frees only the node it is given, not the `next` pointer. This means any prefix of a partially-built linked list can be freed safely during an error mid-parse without double-freeing or leaking. The top-level `free_command_group_list` iterates the list explicitly.

**Token list as a flat null-terminated array.** The lexer produces a `char **` with a separate count, not a linked list. This lets the validator and parser index into it directly with integers, which simplifies the two-pointer (prev / next) lookahead in the validator and the segment-slicing pattern in the parser.

**File redirections override pipes.** Inside each child, pipe `dup2`s happen first and `apply_redirections()` runs after. This means `< file` or `> file` on a command inside a pipeline silently overrides the pipe endpoint on that side, which matches standard shell behaviour.

**prev_fd iteration instead of a pipe array.** The pipeline executor maintains a single integer (`prev_fd`) across the loop rather than pre-allocating an array of `n-1` pipe pairs. Only two file descriptors are live at any point, which simplifies cleanup on fork failure.

**Job table as a compact array with a three-state machine.** Jobs move through `JOB_RUNNING -> JOB_STOPPED -> JOB_RUNNING` (via `bg`/`fg`) or `JOB_RUNNING -> JOB_DONE` (harvested by `check_bg_jobs`). Storing state explicitly means `activities` and `fg`/`bg` can make decisions without re-querying the kernel. On removal, entries are shifted left so the array stays dense and iteration stays a plain `for` loop with no gap-skipping.

**Process group per command.** Every foreground command, including single commands with no pipeline, gets its own process group via `setpgid(0, 0)` in the child and `setpgid(pid, pid)` in the parent (both calls, to avoid a race). This ensures `Ctrl-Z` stops the right group regardless of whether the command was a pipeline or a single binary.

**log.txt excluded from its own history.** Any command line containing the string `log` is not recorded. This prevents the history from filling up with `log execute` calls that would recurse if re-run.

---

## Source Layout

```
C-Shell/
    Makefile
    include/
        globals.h           shared constants, extern declarations
        ast.h               Command / Pipeline / CommandGroup structs
        execute.h / jobs.h / lexer.h / parse.h / prompt.h / signals.h / input.h
        builtins/           one header per builtin
    src/
        main.c              REPL loop, signal setup, variable definitions
        ast.c               node allocation and recursive free functions
        execute.c           fork/exec engine, builtin dispatch, pipe wiring
        input.c             fgets line reader with EINTR and EOF handling
        jobs.c              job table add / remove / find helpers
        lexer.c             character-level tokeniser
        parse.c             validator and recursive descent parser
        prompt.c            prompt rendering with ~ substitution
        signals.c           SIGINT/SIGTSTP handlers, check_bg_jobs
        builtins/           hop, reveal, log, activities, ping, fg, bg
```
