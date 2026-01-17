# C-Shell

A feature-rich Unix shell implementation in C with AST-based parsing, comprehensive job control, and robust signal handling.

## 🌟 What Makes This Shell Special

### Unique Features
- **AST-Based Command Parsing** - Unlike basic tokenization, commands are parsed into an Abstract Syntax Tree for sophisticated execution logic
- **Recursive Descent Parser** - Grammar validation happens before execution, catching syntax errors early
- **Proper Process Groups** - Each command gets its own process group for professional-grade signal handling and TTY control
- **Intelligent Job Control** - Tracks up to 128 jobs with unique monotonic job numbers, automatic zombie reaping, and state monitoring via `/proc`
- **Signal Forwarding Architecture** - SIGINT/SIGTSTP properly forwarded to foreground process groups, shell remains resilient
- **Smart Command History** - Ignores duplicates, supports re-execution, and maintains rolling 15-command log
- **Flexible Redirection Syntax** - Handles both `>file` and `> file` formats seamlessly

## Built-in Commands

| Command | Description | Implementation |
|---------|-------------|----------------|
| `hop [path...]` | Directory navigation with `~`, `-`, `..` support | [B1.c](src/B1.c) |
| `reveal [-a] [-l] [path]` | File listing with flags and sorting | [B2.c](src/B2.c) |
| `log [purge\|execute <n>]` | Command history management (15 entries) | [shell.c](src/shell.c#L234-L263) |
| `ping <pid> <sig>` | Send signals to processes (sig % 32) | [ping.c](src/ping.c) |
| `activities` | List background jobs (sorted, with states) | [shell.c](src/shell.c#L420-L457) |
| `fg [job_num]` | Bring job to foreground | [shell.c](src/shell.c#L471-L499) |
| `bg [job_num]` | Resume stopped job in background | [shell.c](src/shell.c#L501-L518) |

## Core Features

### Process Management
- Foreground/background execution with `&` operator
- Process group management for proper signal delivery
- Automatic cleanup of finished background jobs
- Terminal ownership control (tcsetpgrp)

### Piping & Redirection
- Multi-stage pipelines: `cmd1 | cmd2 | cmd3`
- Input/output/append redirection: `<`, `>`, `>>`
- Combined operations with proper fd management
- Works with both built-in and external commands

### Signal Handling
- **Ctrl+C (SIGINT)** - Forwards to foreground, shell survives
- **Ctrl+Z (SIGTSTP)** - Stops foreground job, moves to background
- Ignores SIGTTIN, SIGTTOU, SIGQUIT for robustness
- Automatic SIGCHLD handling for job cleanup

### Command Chaining
- Sequential: `cmd1 ; cmd2`
- Background: `cmd1 & cmd2`
- Combines with pipes and redirection

## Architecture

**Tokenization** → **AST Construction** → **Syntax Validation** → **Execution**

- [tokenize.c](src/tokenize.c) - Splits input, handles `;`, `|`, `&`
- [ast.c](src/ast.c) - Builds linked Command structures with metadata
- [A3.c](src/A3.c) - Recursive descent parser validates grammar
- [shell.c](src/shell.c) - Execution engine with fork/exec/wait logic

## Building & Running

```bash
make              # Compile (auto-organizes src/ and include/)
./shell.out       # Run shell
make clean        # Clean build artifacts
```

**Makefile Features**: Auto-organization, C99 with POSIX extensions, strict warnings (`-Werror`), dependency tracking

## Project Structure

```
├── Makefile          # Smart build system
├── include/          # Headers (A1.h, A3.h, ast.h, B1.h, B2.h, shell.h, tokenize.h)
└── src/              # Implementation (A1.c, A3.c, ast.c, B1.c, B2.c, ping.c, shell.c, tokenize.c)
```

## Technical Highlights

### Advanced Implementation Details
- **Process Groups**: Each command gets `setpgid(0, 0)` for independent signal control
- **TTY Handoff**: `tcsetpgrp()` passes terminal to foreground jobs, background jobs redirected to `/dev/null`
- **State Detection**: Reads `/proc/<pid>/stat` to distinguish Running vs Stopped jobs
- **Error Codes**: Returns 127 (not found) vs 126 (no permission) per POSIX conventions
- **File Descriptor Safety**: Saves/restores stdin/stdout around redirections
- **Zombie Prevention**: `waitpid(..., WNOHANG)` + cleanup passes prevent orphans

### Quality Assurance
- Strict compilation flags catch errors early
- Syntax validation before execution
- Proper signal handler re-registration
- Graceful handling of invalid inputs

## Example Usage

```bash
# Job control
sleep 30 &                    # Background job
activities                    # List jobs
fg 1                          # Bring to foreground
# Press Ctrl+Z to stop
bg 1                          # Resume in background

# Piping & redirection
cat file.txt | grep "error" | wc -l > count.txt
reveal -al ~ | grep ".c" >> sources.txt

# Command history
log                           # Show history
log execute 3                 # Run 3rd recent command
log purge                     # Clear history
```

## Constraints

- Max 128 background jobs, 15 command history entries, 1000 tokens per line
- Built-in `hop` doesn't work in pipelines (requires parent process for directory change)

---

**Built with**: C99, POSIX.1-2008, Linux system calls
