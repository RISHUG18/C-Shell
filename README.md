# C-Shell

A feature-rich POSIX-compliant shell written in C from scratch — with AST-based parsing, comprehensive job control, and robust signal handling. Zero external dependencies beyond the standard POSIX interface.

---

## What Makes This Shell Special

### Unique Features

- **AST-Based Command Parsing** — Input is parsed into a three-level Abstract Syntax Tree (`Command → Pipeline → CommandGroup`) rather than executed directly from tokens, enabling clean separation of parsing and execution logic.
- **Validate-Before-Allocate** — A dedicated validator pass runs over the flat token array and rejects all malformed input *before* any heap allocation begins, eliminating an entire class of partial-allocation bugs.
- **Proper Process Groups** — Every foreground command (single or pipeline) gets its own process group via a double `setpgid` call (parent + child) to close the race window between `fork` and group assignment.
- **Professional TTY Control** — The shell opens `/dev/tty` explicitly at startup so terminal handoff via `tcsetpgrp` works correctly even when stdin is redirected.
- **Intelligent Job Control** — Tracks up to 128 jobs with monotonic job numbers, compact array storage (no gap-skipping), and automatic zombie reaping before every prompt via `waitpid(..., WNOHANG | WUNTRACED)`.
- **Smart Signal Architecture** — Uses `sigaction(2)` (not `signal`) to control `SA_RESTART` per-signal: `SIGINT` restarts `fgets`, `SIGTSTP` does not (so the prompt redisplays immediately after `Ctrl-Z`).
- **Smart Command History** — Word-boundary filtering prevents commands like `catalog` from being accidentally suppressed; consecutive duplicates are not stored; rolling cap of 15 entries.

---

## Built-in Commands

| Command | Description | Source |
|---------|-------------|--------|
| `hop [path...]` | Directory navigation with `~`, `-`, `.`, `..` and multi-arg support | `builtins/hop.c` |
| `reveal [-a] [-l] [path]` | File listing with flags, `lstat`-based long format, alphabetic sort | `builtins/reveal.c` |
| `log [purge\|execute <N>]` | Persistent 15-entry history with re-execution support | `builtins/log.c` |
| `ping <pid> <signal>` | Send signals to processes (signal taken mod 32) | `builtins/ping.c` |
| `activities` | List all background jobs sorted alphabetically by name | `builtins/activities.c` |
| `fg [N]` | Bring job N to foreground with full terminal handoff | `builtins/fg.c` |
| `bg [N]` | Resume stopped job N in background via `SIGCONT` | `builtins/bg.c` |
| `exit` | Exit the shell | `execute.c` |

---

## Core Features

### Process Management

- Foreground and background execution via `;` and `&` operators
- Each command gets its own process group for independent signal delivery
- `waitpid(..., WNOHANG | WUNTRACED)` before every prompt automatically harvests finished jobs and detects newly stopped ones
- Terminal ownership (`tcsetpgrp`) is explicitly transferred to foreground jobs and reclaimed by the shell on return
- Background jobs have stdin redirected to `/dev/null` to prevent accidental terminal reads

### Piping & Redirection

- Multi-stage pipelines: `cmd1 | cmd2 | cmd3 | ...`
- Input / output / append redirection: `<`, `>`, `>>`
- Operators recognised without surrounding spaces (e.g. `cmd>file` works)
- File redirections inside a pipeline override the pipe endpoint on that side — matching standard shell behaviour
- A single `prev_fd` integer carries the read-end across the fork loop (no pipe array `malloc`)

### Signal Handling

| Signal | Disposition | Effect |
|--------|-------------|--------|
| `SIGINT` | `SA_RESTART` handler | Shell ignores; kernel forwards to foreground process group; `fgets` retries |
| `SIGTSTP` | No-restart handler | Shell ignores; kernel forwards to foreground process group; `fgets` returns `EINTR` → prompt redisplays |
| `SIGTTOU` | `SIG_IGN` | Background processes writing to terminal don't stall |
| `SIGTTIN` | `SIG_IGN` | Background processes reading terminal don't stall |
| `SIGCHLD` | `SIG_DFL` | Default reaping, supplemented by explicit `waitpid` calls |

### Command Chaining

- Sequential execution: `cmd1 ; cmd2`
- Background execution: `cmd1 & cmd2`
- Fully composable with pipes and redirections: `cat file | grep err & wc -l > out.txt`

---

## Architecture

```
Raw Input
    │
    ▼
Lexer (lexer.c)
  Character-level scan → TokenList (flat char** + count)
  Operators tokenised without requiring surrounding spaces
  ">>" detected as a two-character token in the scan itself
    │
    ▼
Validator (parse.c)
  Single pass over token array — no allocation yet
  Rejects: leading/trailing operators, empty pipe sides,
           consecutive separators, redirect without target
    │
    ▼
Parser (parse.c)
  Recursive descent → three-level AST
    CommandGroup  (separated by ; or &, carries is_background flag)
      └─ Pipeline  (connected by |, carries cmd_count)
           └─ Command  (argv[], input_file, output_file, append)
    │
    ▼
Executor (execute.c)
  Built-in dispatch → runs in shell process (affects cwd, job table)
  External commands → fork/exec with process group + tcsetpgrp
  Pipelines → one fork per command, prev_fd pattern, all waited together
```

---

## Building & Running

```bash
make          # Compile with strict flags, outputs shell.out
./shell.out   # Run the shell
make clean    # Remove build/ and shell.out
```

**Compiler flags:**
```
-std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
-Wall -Wextra -Werror -Wno-unused-parameter -fno-asm -g
```

`-Werror` means the project compiles with **zero warnings**. The Makefile uses `-MMD -MP` for auto-generated dependency tracking — incremental rebuilds are always correct.

---

## Project Structure

```
C-Shell/
├── Makefile
├── include/
│   ├── globals.h          Shared constants (MAX_INPUT=4096, MAX_PATH=4096, …) + extern declarations
│   ├── ast.h              Command / Pipeline / CommandGroup structs + alloc/free prototypes
│   ├── execute.h          Execution engine prototypes + init_execute
│   ├── input.h            InputStatus enum + read_input
│   ├── jobs.h             Job struct, JobState enum, MAX_JOBS=128, table helpers
│   ├── lexer.h            TokenList struct + lexer_tokenise / free_token_list
│   ├── parse.h            parse() entry point
│   ├── prompt.h           print_prompt + get_display_cwd
│   ├── signals.h          setup_signals + check_bg_jobs
│   └── builtins/
│       ├── hop.h  reveal.h  log.h  activities.h  ping.h  fg.h  bg.h
└── src/
    ├── main.c             REPL loop: read → log → lex → parse → execute → free
    ├── ast.c              calloc-based alloc_* and recursive free_* implementations
    ├── execute.c          fork/exec engine, builtin dispatch, pipe wiring, apply_redirections
    ├── input.c            fgets reader — classifies EOF / EINTR / blank / OK
    ├── jobs.c             add_job, remove_job (left-shift), find_job_by_num/pid
    ├── lexer.c            Character scanner with operator and >> detection
    ├── parse.c            Token validator + recursive-descent parser
    ├── prompt.c           Live getcwd + ~ substitution on every prompt
    ├── signals.c          sigaction setup for SIGINT/SIGTSTP + check_bg_jobs reaper
    └── builtins/
        ├── hop.c          cd with ~, ., .., -, and multi-argument left-to-right processing
        ├── reveal.c       ls with -a/-l, lstat-based long format, alphabetic qsort
        ├── log.c          15-entry rolling history, word-boundary log filter, re-execution
        ├── activities.c   Background job listing sorted by name via qsort
        ├── ping.c         kill(2) wrapper with modulo-32 signal clamping
        ├── fg.c           Foreground promotion with tcsetpgrp handoff + WUNTRACED wait
        └── bg.c           Background resume via SIGCONT without terminal transfer
```

---

## Technical Highlights

### Advanced Implementation Details

- **Double `setpgid` (parent + child):** Eliminates the race between `fork` returning and the child being placed in its group — whichever side runs first wins; the second call is a benign no-op.
- **TTY via `/dev/tty`:** Shell opens the controlling terminal explicitly rather than assuming fd 0 is a terminal, so it works correctly when stdin is redirected.
- **Redirect-overrides-pipe:** `apply_redirections()` runs *after* the pipe `dup2`s inside each child, so `cmd1 | cmd2 > file` correctly redirects `cmd2`'s stdout to `file` rather than the pipe — standard POSIX behaviour.
- **POSIX error codes:** `exec` failures return 127 (command not found / `ENOENT`) or 126 (permission denied / `EACCES` / `ENOEXEC`).
- **Word-boundary log filter:** `log_record` scans for every occurrence of the substring `log` and only suppresses recording if it appears as a standalone word. Commands like `catalog` are recorded normally.
- **Recursive-safe free:** Every `free_*` function frees only its own node, not `->next`. Any prefix of a partially-built linked list can be freed safely on a mid-parse allocation failure.

### Example Usage

```bash
# Job control
sleep 60 &           # Start background job — prints [1] <pid>
activities           # [1] <pid> - Running - sleep
fg 1                 # Bring to foreground; Ctrl-Z to stop
bg 1                 # Resume in background

# Piping & redirection
cat file.txt | grep "error" | wc -l > count.txt
reveal -al ~ | grep "\.c"

# History
log                  # Print history (oldest first)
log execute 2        # Re-run the 2nd most recent command
log purge            # Clear history

# Chaining
make all ; ./shell.out
```

---

## Constraints

- Max **128** concurrent background jobs (`MAX_JOBS`)
- Max **15** command history entries (`LOG_MAX_ENTRIES`)
- Max **4096** characters per input line (`MAX_INPUT`)
- `hop` (like any `cd`) only affects the shell process — it cannot change directories inside a pipeline stage

**Built with:** C99 · POSIX.1-2008 · Linux system calls · GCC
