# C-Shell

A custom POSIX-compliant interactive shell written in C from scratch. The entire shell — lexer, parser, AST, executor, job control, and all built-ins — is implemented across ~1 500 lines of pure C with **zero external dependencies** beyond the standard POSIX interface. Every subsystem lives in its own translation unit, making the codebase easy to navigate and extend.

---

## Building and Running

```
cd C-Shell
make all
./shell.out
```

`make clean` removes all build artefacts (the `build/` directory and `shell.out`). The Makefile uses `gcc` with auto-dependency tracking (`-MMD -MP`) so incremental rebuilds are correct. Compiler flags enforced are:

```
-std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
-Wall -Wextra -Werror -Wno-unused-parameter -fno-asm -g
```

The `-Werror` flag means the project compiles with zero warnings.

---

## Prompt

```
<username@hostname:cwd>
```

`username` is resolved via `getpwuid(getuid())` and `hostname` via `gethostname()`. The directory where the shell was launched is treated as home (`shell_home`). Any path that descends from it is displayed with the leading portion replaced by `~`. This substitution is recalculated on every prompt using a fresh `getcwd()` call rather than a cached value, so it stays correct after `chdir` calls from any source.

---

## Input

Lines are read with `fgets` into a fixed `MAX_INPUT` (4096-byte) buffer. Trailing newlines and carriage returns are stripped. Blank or whitespace-only input returns `INPUT_BLANK` and is silently ignored. `Ctrl-D` (EOF) causes `fgets` to return `NULL`; the shell detects this via `feof()`, prints a newline, and exits cleanly. Signal interrupts (`EINTR`) cause `fgets` to return `NULL` without setting EOF; the shell calls `clearerr(stdin)` and returns `INPUT_RETRY`, which redisplays the prompt without printing an error.

---

## Parsing Pipeline

Input goes through three stages before anything is executed:

1. **Lexer** (`lexer.c`) — character-level scan that splits the line into a `TokenList` (a `NULL`-terminated `char **` with a count). Operators (`|`, `;`, `&`, `<`, `>`, `>>`) are recognised without needing surrounding spaces. `>>` is identified as a two-character token during the scan itself so the parser never has to peek ahead for it.

2. **Validator** — a single-pass check over the flat token array before any heap allocation. Rejects: operators at the start or end of input, consecutive `;`/`&` separators, empty pipe sides (`|` adjacent to another operator), and redirect operators without a following filename target. Prints `Invalid Syntax!` and aborts if any rule is violated.

3. **Parser** (`parse.c`) — builds the Abstract Syntax Tree (AST). The tree has three levels:
   - `Command` — `argv` array, `argc`, optional `input_file` / `output_file`, `append` flag.
   - `Pipeline` — linked list of `Command` nodes connected by `|`, plus a `cmd_count`.
   - `CommandGroup` — a `Pipeline` with an `is_background` flag, linked to the next group.

   Groups are separated by `;` or `&`. The parser walks each command's token range twice: once to count `argv` slots, once to populate them and extract redirections. All AST nodes are heap-allocated with `calloc` (zeroing all fields). Deallocation is recursive — `free_command_group_list` iterates the list explicitly and calls `free_command_group` → `free_pipeline` → `free_command`, each freeing only the node it receives so partial lists can be cleaned up safely on mid-parse allocation failure.

---

## Execution

### Foreground

Each foreground command runs in its own process group (`setpgid(0,0)` in the child, `setpgid(pid,pid)` in the parent — both calls to eliminate the race). The shell opens `/dev/tty` at startup and hands terminal control to that group with `tcsetpgrp`. It then waits using `waitpid(..., WUNTRACED)`. If the process stops (`WIFSTOPPED`), it is registered in the job table as `JOB_STOPPED` and the shell reclaims the terminal. On normal exit the terminal is returned without a job table entry.

### Pipelines

One child is forked per command. Between each pair of adjacent commands a `pipe()` pair is created. A single `prev_fd` integer carries the read-end across loop iterations and is wired to the next child's stdin via `dup2`. All pipe write-ends and the previous read-end are closed in the parent immediately after each fork to avoid descriptor leaks. All children are waited on together after all forks complete. File redirections applied inside the child (`apply_redirections()`) run **after** the pipe `dup2`s, so `< file` or `> file` on a command inside a pipeline silently overrides that pipe endpoint — matching standard shell behaviour.

### Background (`&`)

Background command groups are run in a dedicated child process that itself calls `setpgid(0,0)` and redirects stdin to `/dev/null`. The shell does not wait for this child. The PID and an auto-incremented job number are registered in the in-memory job table and `[N] pid` is printed to stdout.

---

## Built-in Commands

Built-ins execute directly in the shell process so they can mutate shell state (working directory, job table, history file).

### `hop [path...]`

`cd` equivalent. With no arguments, changes to `shell_home`. Accepts one or more arguments processed left to right. Special tokens:

| Token | Behaviour |
|-------|-----------|
| `~`   | Change to `shell_home` |
| `.`   | Stay in place (records `prev_cwd`) |
| `..`  | Go up one level |
| `-`   | Toggle to the previous directory |
| *path*| Absolute or relative path |

### `reveal [-a] [-l] [path]`

Directory listing. Flags `-a` (show hidden entries) and `-l` (long format) may be combined (e.g. `-la`). Accepts at most one path argument; uses the current directory if none is given. Supports the same special tokens as `hop` (`~`, `.`, `..`, `-`).

Long format (`-l`) outputs one line per entry:
```
<perms> <nlink> <owner> <group> <size> <mtime> <name>
```
Entries are collected with `readdir`, sorted alphabetically with `qsort` + `strcmp`, then printed. Symlinks, directories, character devices, block devices, FIFOs, and sockets each get their correct type character in the permission string.

### `log`

Persistent command history backed by `log.txt`, capped at **15** entries.

| Subcommand | Behaviour |
|------------|-----------|
| `log` | Print history, oldest entry first |
| `log purge` | Clear the history file |
| `log execute N` | Re-run the N-th most recent entry (1 = most recent) |

**Filtering rules (applied before recording):**
- Commands where `log` appears as a standalone word (surrounded by spaces, tabs, or string boundaries) are **not** recorded. This prevents the history from filling with `log execute` calls that would recurse if re-run.
- Consecutive duplicate commands are not recorded.
- When the log is full, the oldest entry is dropped before appending the new one.

### `activities`

Prints all running and stopped background jobs sorted alphabetically by command name. Output format per job:
```
[job_num] pid - Running|Stopped - name
```

### `ping <pid> <signal>`

Sends a signal to a process. Both arguments must be non-negative integers. The signal number is taken **modulo 32** before the `kill(2)` call. On success prints:
```
Sent signal <signal> to process with pid <pid>
```
Reports `no such process` if the target PID does not exist.

### `fg [N]`

Brings job N (default 1) to the foreground. Hands terminal control to the job's process group, sends `SIGCONT`, and waits with `WUNTRACED`. If the job stops again, it stays in the job table as `JOB_STOPPED`. On normal exit the job is removed from the table.

### `bg [N]`

Resumes stopped job N (default 1) in the background by sending `SIGCONT` to the process group without granting it the terminal. Reports an error if the job is already running.

### `exit`

Exits the shell immediately.

---

## Job Control

The job table (`jobs[]`) is a compact array of up to `MAX_JOBS` (128) entries. Each entry stores the PID, auto-incremented job number, name (first token of the command), and a three-value state machine:

```
JOB_RUNNING → JOB_STOPPED   (Ctrl-Z / SIGTSTP delivered to fg group)
JOB_STOPPED → JOB_RUNNING   (bg or fg)
JOB_RUNNING → (removed)     (process exits, harvested by check_bg_jobs)
```

Before every prompt, `check_bg_jobs()` calls `waitpid(..., WNOHANG | WUNTRACED)` on each tracked job:
- If the job has exited → print `[N] Done   name` and remove from table.
- If the job has stopped → print `[N] Stopped   name` and mark `JOB_STOPPED`.

On removal, entries are shifted left so the array stays dense and iteration is a simple `for` loop with no gap-skipping.

---

## Signal Handling

| Signal | Disposition |
|--------|-------------|
| `SIGINT` | `SA_RESTART` handler — shell ignores it; kernel forwards to fg process group |
| `SIGTSTP` | No-restart handler — shell ignores it (returns `EINTR` from `fgets` to redisplay prompt); kernel forwards to fg process group |
| `SIGTTOU` | `SIG_IGN` — prevents background processes that accidentally write to the terminal from stalling |
| `SIGTTIN` | `SIG_IGN` — prevents background processes that read the terminal from stalling |
| `SIGCHLD` | `SIG_DFL` — default reaping, overridden by explicit `waitpid` calls |

`sigaction(2)` (not `signal(2)`) is used for `SIGINT` and `SIGTSTP` to get precise control over `SA_RESTART` on a per-signal basis.

---

## Design Choices

**Validate before allocating.** The validator runs a single pass over the flat token array before any AST node is created. This keeps the parser simple — it never has to handle malformed input — and eliminates a whole category of partial-allocation cleanup bugs.

**AST recursive free is safe for partial lists.** Each `free_*` function frees only the node it is given, not the `next` pointer. `free_command_group_list` iterates the list explicitly. This means any prefix of a partially-built linked list can be freed safely on an allocation failure mid-parse without double-freeing or leaking.

**Flat null-terminated token array.** The lexer produces a `char **` with a separate count rather than a linked list. The validator and parser can index into it directly with plain integers, which simplifies the two-pointer (prev/next) lookahead in the validator and the segment-slicing pattern in the parser.

**`prev_fd` instead of a pipe array.** The pipeline executor maintains a single integer (`prev_fd`) across the fork loop rather than pre-allocating an array of `n−1` pipe pairs. Only two file descriptors are live at any point, which simplifies cleanup on fork failure and eliminates a `malloc`.

**Double `setpgid` call (parent + child).** `setpgid(pid, pid)` is called in both the parent and the child immediately after `fork`. Whichever runs first wins; the second call is a benign no-op. This closes the race window where a `SIGINT` delivered between `fork` and `setpgid` could reach the wrong process group.

**`log` word-boundary filtering.** The `log_record` function does not simply check `strstr(line, "log")`. It scans for every occurrence of the substring and only suppresses the record if `log` is surrounded by whitespace or string boundaries. This prevents a command named e.g. `catalog` from being accidentally excluded.

**`/dev/tty` for terminal control.** `init_execute` opens `/dev/tty` explicitly rather than assuming fd 0 is a terminal. This means the shell still runs correctly when stdin is redirected (e.g. in scripts), falling back to `STDIN_FILENO` only if `/dev/tty` cannot be opened.

---

## Source Layout

```
C-Shell/
    Makefile
    include/
        globals.h               shared constants (MAX_INPUT, MAX_PATH, …) and extern declarations
        ast.h                   Command / Pipeline / CommandGroup structs + alloc/free prototypes
        execute.h               execute_single, execute_pipeline, execute_command_group_list, init_execute
        input.h                 InputStatus enum, read_input
        jobs.h                  Job struct, JobState enum, MAX_JOBS, job table helpers
        lexer.h                 TokenList struct, lexer_tokenise, free_token_list
        parse.h                 parse (entry point), validator
        prompt.h                print_prompt, get_display_cwd
        signals.h               setup_signals, check_bg_jobs
        builtins/
            hop.h               builtin_hop
            reveal.h            builtin_reveal
            log.h               log_record, builtin_log, LOG_MAX_ENTRIES, LOG_FILE
            activities.h        builtin_activities
            ping.h              builtin_ping
            fg.h                builtin_fg
            bg.h                builtin_bg
    src/
        main.c                  REPL loop, signal setup, global variable definitions
        ast.c                   alloc_* and recursive free_* implementations
        execute.c               fork/exec engine, builtin dispatch, pipe wiring, apply_redirections
        input.c                 fgets reader with EOF / EINTR / blank-line classification
        jobs.c                  job table: add_job, remove_job, find_job_by_num, find_job_by_pid
        lexer.c                 character-level tokeniser with operator and >> detection
        parse.c                 token validator and recursive-descent group/pipeline/command parser
        prompt.c                prompt rendering with live getcwd + ~ substitution
        signals.c               sigaction setup for SIGINT/SIGTSTP, check_bg_jobs reaper
        builtins/
            hop.c               cd with ~, ., .., - and multi-argument support
            reveal.c            ls with -a/-l, lstat-based long format, alphabetic sort
            log.c               persistent 15-entry history with word-boundary log filtering
            activities.c        sorted background job listing
            ping.c              kill(2) wrapper with modulo-32 signal clamping
            fg.c                foreground promotion with terminal handoff and WUNTRACED wait
            bg.c                background resume via SIGCONT without terminal transfer
```
