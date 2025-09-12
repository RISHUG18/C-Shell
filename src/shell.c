// Include all necessary headers for shell functionality
#include "../include/A1.h"
#include "../include/tokenize.h"
#include "../include/shell.h"
#include "../include/A3.h"
#include "../include/ast.h"
#include "../include/B1.h"
#include <sys/wait.h>
#include "../include/B2.h"
// #include "B3.h"
#include "../include/shell.h"
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
// #include "C2.h"
// #include<pwd.h>
// #include<unistd.h>
    // Forward declarations for custom commands and job cleanup
    void ping_command(const char *pid_str, const char *sig_str);
    void cleanup_bg_jobs(void);
    void cleanup_bgjobs(void);
    // Background job status checker

// Data structures and global variables for job control and shell state
#define MAX_BG_JOBS 128
typedef struct {
    int job_num;           // Unique job number
    pid_t pid;             // Process ID
    char cmd_name[256];    // Command name
    char cmd_line[1024];   // Full command line for printing
    int active;            // 1 if job is active
    int stopped;           // 1 if stopped by SIGTSTP, 0 otherwise
} BgJob;
BgJob bg_jobs[MAX_BG_JOBS];
int bg_job_count = 0;
int next_job_num = 1;      // Monotonic counter for unique job numbers
char* tokens[MAX_TOKENS];
int token_cnt;
char home[1024];           // Home directory
char shellcwd[1024];       // Current working directory
char prevcwd[1024];        // Previous working directory
int prev_set = 0;          // Track if hop has successfully set prevcwd at least once
int child_pid = -1;        // PID of current foreground child

// Print a friendly error when execvp fails
static void report_exec_error(const char *cmd) {
    switch (errno) {
        case ENOENT:
        case ENOTDIR:
            fprintf(stderr, "Command not found!\n");
            break;
        case EACCES:
            fprintf(stderr, "Permission denied!\n");
            break;
        case ENOEXEC:
            fprintf(stderr, "Exec format error!\n");
            break;
        default:
            perror(cmd ? cmd : "execvp");
            break;
    }
}


// Check status of background jobs and clean up finished ones
void check_bg_jobs() {
    for (int i = 0; i < bg_job_count; ++i) {
        if (bg_jobs[i].active) {
            int status;
            pid_t result = waitpid(bg_jobs[i].pid, &status, WNOHANG);
            if (result > 0) {
                // Job finished
                bg_jobs[i].active = 0;
                bg_jobs[i].stopped = 0;
                if (WIFEXITED(status)) {
                    printf("%s with pid %d exited normally\n", bg_jobs[i].cmd_name, bg_jobs[i].pid);
                } else {
                    printf("%s with pid %d exited abnormally\n", bg_jobs[i].cmd_name, bg_jobs[i].pid);
                }
            } else if (result == -1) {
                // Already reaped or no such child -> remove from list
                if (errno == ECHILD || errno == ESRCH) {
                    bg_jobs[i].active = 0;
                    bg_jobs[i].stopped = 0;
                }
            } else { // result == 0 -> not reaped, double-check if process exists
                if (kill(bg_jobs[i].pid, 0) == -1 && errno == ESRCH) {
                    bg_jobs[i].active = 0;
                    bg_jobs[i].stopped = 0;
                }
            }
        }
    }
    // Prune inactive entries
    cleanup_bg_jobs();
}
// Print the shell prompt with current directory or computer name
void print_prompt() {
    char res[1024];
    getcwd(shellcwd, sizeof(shellcwd));
    computername(res, shellcwd);
    printf("<%s> ", res);
    fflush(stdout);
}

// Handler for SIGINT (Ctrl+C)
void sigint_handler(int sig) {
    if (child_pid > 0) {
        // Forward SIGINT to the child process group
        kill(-child_pid, SIGINT);
    } else {
        // If no child, print newline and reprint prompt
        printf("\n");
        // print_prompt();
        fflush(stdout);
    }
    // Re-register the handler to keep shell running
    signal(SIGINT, sigint_handler);
}

    // void tologs(char* intake){
    //     FILE* file=fopen("logs.txt","r");
    //     char history[15][1024];
            // Background job structure
            
    //     int count=0;
    //     if(file){
    //         while(fgets(history[count],1024,file)){
    //             history[count][strcspn(history[count],"\n")]=0;
    //             count++;

    //         if(count>=15){
    //             break;
    //         }
    //         }
    //     }
    //     fclose(file);
    //     FILE* file=fopen("logs.txt","w");
    //     if(count==0){
    //         fwrite(intake,sizeof(char),strlen(intake),file);
    //         return;
    //     }
    //     if(strcmp(history[count-1],intake)==0){
    //         return;
    //     }

    //     // for(int i=)
    // }


// Execute a chain of commands (AST), handling pipes, redirections, and builtins
void execute(Command* ast) {
    int prev_fd = -1;
    int mainin = dup(STDIN_FILENO);
    int mainout = dup(STDOUT_FILENO);

    for (Command* cmd = ast; cmd != NULL; cmd = cmd->next) {
        // Skip empty or pipe tokens
        if (!cmd->args || !cmd->args[0]) continue;
        if (strcmp(cmd->args[0], "|") == 0) continue;

        int pipefd[2];
        int usepipe = (cmd->next && strcmp(cmd->next->args[0], "|") == 0);
        if (usepipe) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                continue;
            }
        }

        // Handle simple command (no pipeline)
        if (!usepipe && prev_fd == -1) {
            int save_in = dup(STDIN_FILENO);
            int save_out = dup(STDOUT_FILENO);
            int redir_failed = 0;

            // Handle input/output redirections
            for (int i = 1; cmd->args[i] != NULL; i++) {
                if (strncmp(cmd->args[i], "<", 1) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 1) ? cmd->args[i + 1] : cmd->args[i] + 1;
                    int fd = open(path, O_RDONLY, 0644);
                    if (fd < 0) { fprintf(stderr, "No such file or directory\n"); redir_failed = 1; break; }
                    dup2(fd, STDIN_FILENO); close(fd); cmd->args[i] = NULL;
                } else if (strncmp(cmd->args[i], ">>", 2) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 2) ? cmd->args[i + 1] : cmd->args[i] + 2;
                    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd < 0) { fprintf(stderr, "Unable to create file for writing\n"); redir_failed = 1; break; }
                    dup2(fd, STDOUT_FILENO); close(fd); cmd->args[i] = NULL;
                } else if (strncmp(cmd->args[i], ">", 1) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 1) ? cmd->args[i + 1] : cmd->args[i] + 1;
                    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) { fprintf(stderr, "Unable to create file for writing\n"); redir_failed = 1; break; }
                    dup2(fd, STDOUT_FILENO); close(fd); cmd->args[i] = NULL;
                }
            }

            if (redir_failed) {
                dup2(save_in, STDIN_FILENO);
                dup2(save_out, STDOUT_FILENO);
                close(save_in); close(save_out);
                continue;
            }

            // Handle built-in commands in parent
            if (strcmp(cmd->name, "hop") == 0) {
                int hop_to_prev = 0;
                for (int h = 1; cmd->args[h] != NULL; h++) {
                    if (strcmp(cmd->args[h], "-") == 0) hop_to_prev = 1;
                }
                if (hop_to_prev && (prevcwd[0] == '\0' || strcmp(prevcwd, ".") == 0)) {
                    // prevcwd is unset or invalid, do nothing
                } else {
                    hop(cmd);
                }
                dup2(save_in, STDIN_FILENO);
                dup2(save_out, STDOUT_FILENO);
                close(save_in); close(save_out);
                continue;
            }
            if (strcmp(cmd->name, "reveal") == 0) {
                reveal(cmd);
                dup2(save_in, STDIN_FILENO);
                dup2(save_out, STDOUT_FILENO);
                close(save_in); close(save_out);
                continue;
            }
            if (strcmp(cmd->args[0], "log") == 0) {
                // Handle log command: print, purge, or execute from log
                char logpath[1050]; snprintf(logpath, sizeof(logpath), "log.txt");
                FILE* fd = fopen(logpath, "r");
                if (!fd) { dup2(save_in, STDIN_FILENO); dup2(save_out, STDOUT_FILENO); close(save_in); close(save_out); continue; }
                char logbuffer[16][1050]; int ln = 0;
                while (ln < 16 && fgets(logbuffer[ln], sizeof(logbuffer[ln]), fd)) ln++;
                fclose(fd);
                if (cmd->args[1] == NULL) {
                    for (int j = 0; j < ln; j++) printf("%s", logbuffer[j]);
                    dup2(save_in, STDIN_FILENO); dup2(save_out, STDOUT_FILENO); close(save_in); close(save_out);
                    continue;
                }
                for (int j = 1; cmd->args[j] != NULL; j++) {
                    if (strcmp(cmd->args[j], "purge") == 0) { int tmp = open(logpath, O_CREAT | O_WRONLY | O_TRUNC, 0644); if (tmp >= 0) close(tmp); }
                    if (strcmp(cmd->args[j], "execute") == 0 && cmd->args[j+1]) {
                        int idx = atoi(cmd->args[j+1]);
                        if (idx > 0 && idx <= ln) {
                            tokenize_input(logbuffer[ln - idx]);
                            Command* sub = parse_tokens();
                            execute(sub);
                        }
                    }
                }
                dup2(save_in, STDIN_FILENO); dup2(save_out, STDOUT_FILENO); close(save_in); close(save_out);
                continue;
            }

            // External command: fork and exec
            pid_t cpid = fork();
            if (cpid == 0) {
                execvp(cmd->args[0], cmd->args);
                report_exec_error(cmd->args[0]);
                int code = (errno == ENOENT || errno == ENOTDIR) ? 127 : (errno == EACCES ? 126 : 1);
                _exit(code);
            } else if (cpid > 0) {
                waitpid(cpid, NULL, 0);
            } else {
                perror("fork");
            }

            dup2(save_in, STDIN_FILENO);
            dup2(save_out, STDOUT_FILENO);
            close(save_in); close(save_out);
            continue;
        }

        // Handle pipelined commands or previous input
        pid_t pid = fork();
        if (pid == 0) {
            // CHILD
            if (prev_fd != -1) { dup2(prev_fd, STDIN_FILENO); close(prev_fd); }
            if (usepipe) { dup2(pipefd[1], STDOUT_FILENO); close(pipefd[0]); close(pipefd[1]); }

            // Redirections in child
            for (int i = 1; cmd->args[i] != NULL; i++) {
                if (strncmp(cmd->args[i], "<", 1) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 1) ? cmd->args[i + 1] : cmd->args[i] + 1;
                    int fd = open(path, O_RDONLY, 0644);
                    if (fd < 0) { fprintf(stderr, "No such file or directory\n"); _exit(1); }
                    dup2(fd, STDIN_FILENO); close(fd); cmd->args[i] = NULL;
                } else if (strncmp(cmd->args[i], ">>", 2) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 2) ? cmd->args[i + 1] : cmd->args[i] + 2;
                    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd < 0) { fprintf(stderr, "Unable to create file for writing\n"); _exit(1); }
                    dup2(fd, STDOUT_FILENO); close(fd); cmd->args[i] = NULL;
                } else if (strncmp(cmd->args[i], ">", 1) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 1) ? cmd->args[i + 1] : cmd->args[i] + 1;
                    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) { fprintf(stderr, "Unable to create file for writing\n"); _exit(1); }
                    dup2(fd, STDOUT_FILENO); close(fd); cmd->args[i] = NULL;
                }
            }

            // Builtins in pipeline run in child
            if (strcmp(cmd->args[0], "reveal") == 0) { reveal(cmd); _exit(0); }
            if (strcmp(cmd->args[0], "hop") == 0) { _exit(1); }
            if (strcmp(cmd->args[0], "log") == 0) {
                char logpath[1050]; snprintf(logpath, sizeof(logpath), "log.txt");
                FILE* fd = fopen(logpath, "r");
                if (fd) {
                    char logbuffer[16][1050]; int ln = 0;
                    while (ln < 16 && fgets(logbuffer[ln], sizeof(logbuffer[ln]), fd)) ln++;
                    fclose(fd);
                    if (cmd->args[1] == NULL) {
                        for (int j = 0; j < ln; j++) printf("%s", logbuffer[j]);
                    } else {
                        for (int j = 1; cmd->args[j] != NULL; j++) {
                            if (strcmp(cmd->args[j], "purge") == 0) { int tmp = open(logpath, O_CREAT | O_WRONLY | O_TRUNC, 0644); if (tmp >= 0) close(tmp); }
                            if (strcmp(cmd->args[j], "execute") == 0 && cmd->args[j+1]) {
                                int idx = atoi(cmd->args[j+1]);
                                if (idx > 0 && idx <= ln) {
                                    tokenize_input(logbuffer[ln - idx]);
                                    Command* sub = parse_tokens();
                                    execute(sub);
                                }
                            }
                        }
                    }
                }
                _exit(0);
            }

            // External command in pipeline
            execvp(cmd->args[0], cmd->args);
            report_exec_error(cmd->args[0]);
            {
                int code = (errno == ENOENT || errno == ENOTDIR) ? 127 : (errno == EACCES ? 126 : 1);
                _exit(code);
            }
        } else if (pid > 0) {
            // PARENT
            int status = 0;
            waitpid(pid, &status, 0);
            if (prev_fd != -1) { close(prev_fd); }
            if (usepipe) {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            } else {
                prev_fd = -1;
            }
        } else {
            perror("fork");
            if (usepipe) { close(pipefd[0]); close(pipefd[1]); }
        }
    }

    // Cleanup and restore original stdin/stdout
    if (prev_fd != -1) close(prev_fd);
    dup2(mainin, STDIN_FILENO);
    dup2(mainout, STDOUT_FILENO);
    close(mainin); close(mainout);
}
    // Helper for sorting jobs lexicographically by command name
int cmp_bgjob(const void *a, const void *b) {
    const BgJob *ja = *(const BgJob **)a;
    const BgJob *jb = *(const BgJob **)b;
    return strcmp(ja->cmd_name, jb->cmd_name);
}

// Returns "Running" or "Stopped" for a given pid
const char* get_job_state(pid_t pid) {
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/stat", pid);
    FILE *fp = fopen(proc_path, "r");
    if (!fp) return "Stopped"; // If proc not found, treat as stopped

    int pid_read = 0;
    char comm[256] = {0};
    char state = 'R';
    // Correctly parse: pid (comm) state ...
    if (fscanf(fp, "%d (%255[^)]) %c", &pid_read, comm, &state) != 3) {
        fclose(fp);
        return "Stopped";
    }
    fclose(fp);

    if (state == 'T') return "Stopped";
    return "Running";
}

// Remove inactive jobs from bg_jobs array
void cleanup_bg_jobs() {
    int j = 0;
    for (int i = 0; i < bg_job_count; ++i) {
        if (bg_jobs[i].active) {
            bg_jobs[j++] = bg_jobs[i];
        }
    }
    bg_job_count = j;
}
// Alias for any places that call the misspelled name
void cleanup_bgjobs() {
    cleanup_bg_jobs();
}

//llm code starts//
// The activities command implementation
void activities() {
    // First, update job status and remove finished jobs
    for (int i = 0; i < bg_job_count; ++i) {
        if (!bg_jobs[i].active) continue;

        int status;
        pid_t result = waitpid(bg_jobs[i].pid, &status, WNOHANG);
        if (result > 0) {
            bg_jobs[i].active = 0; // child reaped -> remove
            bg_jobs[i].stopped = 0;
        } else if (result == -1) {
            if (errno == ECHILD || errno == ESRCH) {
                bg_jobs[i].active = 0; // no such child/process -> remove
                bg_jobs[i].stopped = 0;
            }
        } else { // result == 0
            if (kill(bg_jobs[i].pid, 0) == -1 && errno == ESRCH) {
                bg_jobs[i].active = 0; // process no longer exists -> remove
                bg_jobs[i].stopped = 0;
            }
        }
    }
    cleanup_bg_jobs();

    // Prepare array of pointers for sorting
    BgJob* job_ptrs[MAX_BG_JOBS];
    int count = 0;
    for (int i = 0; i < bg_job_count; ++i) {
        if (bg_jobs[i].active) {
            job_ptrs[count++] = &bg_jobs[i];
        }
    }
    qsort(job_ptrs, count, sizeof(BgJob*), cmp_bgjob);

    // Print jobs in required format
    for (int i = 0; i < count; ++i) {
        const char* state;
        if (job_ptrs[i]->stopped) {
            state = "Stopped";
        } else {
            state = get_job_state(job_ptrs[i]->pid);
        }
        printf("[%d] : %s - %s\n", job_ptrs[i]->pid, job_ptrs[i]->cmd_name, state);
    }
}
//llm code ends//

//llm code starts//
volatile sig_atomic_t fg_stopped = 0;
volatile sig_atomic_t fg_pid = -1;
char fg_cmd_name[256] = "";
char fg_cmd_line[1024] = ""; // full command line for current foreground chain

// ===== Helpers for fg/bg =====
static void build_cmd_line_from_args(char *dst, size_t n, char **args) {
    if (!dst || n == 0) return;
    dst[0] = '\0';
    if (!args) return;
    size_t left = n;
    for (int i = 0; args[i]; i++) {
        if (i) { strncat(dst, " ", left > 0 ? left - 1 : 0); }
        strncat(dst, args[i], left > 0 ? left - 1 : 0);
        left = n - strlen(dst);
        if (left <= 1) break;
    }
}

static int find_job_index_by_num(int job_num) {
    for (int i = 0; i < bg_job_count; ++i) {
        if (bg_jobs[i].active && bg_jobs[i].job_num == job_num) return i;
    }
    return -1;
}

static int find_mru_job_index(void) {
    // Choose the active job with the highest job_num to mimic typical shell behavior
    int idx = -1;
    int best_job_num = -1;
    for (int i = 0; i < bg_job_count; ++i) {
        if (bg_jobs[i].active && bg_jobs[i].job_num > best_job_num) {
            best_job_num = bg_jobs[i].job_num;
            idx = i;
        }
    }
    return idx;
}

static int is_number_str(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; ++p) if (*p < '0' || *p > '9') return 0;
    return 1;
}

static void fg_builtin(int have_arg, int job_num) {
    // Refresh jobs and validate
    int idx = have_arg ? find_job_index_by_num(job_num) : find_mru_job_index();
    if (idx == -1) { printf("No such job\n"); return; }
    BgJob *job = &bg_jobs[idx];

    const char *print_cmd = job->cmd_line[0] ? job->cmd_line : job->cmd_name;
    printf("%s\n", print_cmd);

    // Resume if stopped (send to process group)
    if (job->stopped) {
        if (kill(-job->pid, SIGCONT) == -1) {
            if (errno == ESRCH) { printf("No such job\n"); job->active = 0; job->stopped = 0; cleanup_bg_jobs(); return; }
        } else {
            job->stopped = 0;
        }
    }

    child_pid = job->pid;
    int status;
    // Wait until it exits or stops again
    while (1) {
        pid_t w = waitpid(job->pid, &status, WUNTRACED);
        if (w == -1) {
            if (errno == EINTR) continue;
            if (errno == ECHILD || errno == ESRCH) { job->active = 0; job->stopped = 0; }
            break;
        }
        if (WIFSTOPPED(status)) { job->stopped = 1; job->active = 1; break; }
        if (WIFEXITED(status) || WIFSIGNALED(status)) { job->active = 0; job->stopped = 0; break; }
    }
    child_pid = -1;
    cleanup_bg_jobs();
}

static void bg_builtin(int have_arg, int job_num) {
    int idx = have_arg ? find_job_index_by_num(job_num) : find_mru_job_index();
    if (idx == -1) { printf("No such job\n"); return; }
    BgJob *job = &bg_jobs[idx];

    if (!job->stopped) {
        const char *st = get_job_state(job->pid);
        if (strcmp(st, "Stopped") != 0) { printf("Job already running\n"); return; }
    }

    if (kill(-job->pid, SIGCONT) == -1) {
        if (errno == ESRCH) { printf("No such job\n"); }
        else perror("bg SIGCONT");
        return;
    }
    job->stopped = 0;
    const char *name_for_bg = job->cmd_name[0] ? job->cmd_name : (job->cmd_line[0] ? job->cmd_line : "job");
    printf("[%d] %s\n", job->job_num, name_for_bg);
}
//llm code ends//

//llm code starts//
// SIGTSTP handler
void sigtstp_handler(int sig) {
    if (child_pid > 0) {
        // Send SIGTSTP to the foreground process group
        kill(-child_pid, SIGTSTP);
        fg_stopped = 1;
        fg_pid = child_pid;
        // fg_cmd_name is set before forking
    }
    // Reinstall handler
    signal(SIGTSTP, sigtstp_handler);
}

//llm code ends//
// Main shell loop: handles prompt, input, parsing, execution, job control
int main(){

        // char home[1024];
        if (signal(SIGINT, sigint_handler) == SIG_ERR) {
            perror("signal");
            exit(1);
        }
        // Install SIGTSTP handler
    if (signal(SIGTSTP, sigtstp_handler) == SIG_ERR) {
        perror("signal SIGTSTP");
        exit(1);
    }
    // Avoid shell being stopped by terminal I/O while handing off ttys
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    if (getcwd(home, sizeof(home)) == NULL) {
        strcpy(home, "."); // fallback to current directory
    }
    if (getcwd(prevcwd, sizeof(prevcwd)) == NULL) {
        strcpy(prevcwd, ".");
    }
    // char shellcwd[1024];        //cwd of c shell
    strcpy(shellcwd, home);
        while(1){
        //// A1 starts  /////
    print_prompt();
        ///// A1 ends  /////


        ///// A2 stats /////
        char intake[1050];
        
        // Use fgets instead of scanf for better signal handling
        if (fgets(intake, sizeof(intake), stdin) == NULL) {
            // Handle Ctrl+C or EOF - but don't exit on Ctrl+C
            if (feof(stdin)) {
                // EOF: exit only if no foreground child is running
                if (child_pid <= 0) {
                    for (int j = 0; j < bg_job_count; ++j) {
                        if (bg_jobs[j].active) {
                            kill(bg_jobs[j].pid, SIGKILL);
                        }
                    }
                    printf("logout\n");
                    exit(0);
                }
                clearerr(stdin);
                continue;
            }
            // If interrupted by signal (Ctrl+C), just continue to next prompt
            clearerr(stdin);
            continue;
        }
        
        // Remove newline from fgets
        intake[strcspn(intake, "\n")] = 0;
        
        // Check if input is empty or just whitespace
        int non_space = 0;
        for (int k = 0; intake[k] != '\0'; k++) {
            if (intake[k] != ' ' && intake[k] != '\t') {
                non_space = 1;
                break;
            }
        }
        if (!non_space) {
            continue; // Skip empty input
        }
        
        char logpath[1050];
        snprintf(logpath,sizeof(logpath),"log.txt");
        FILE* fp = fopen(logpath,"r");
        char logbuffer[16][1024];
        int i=0;
        if (fp) {
            while(i < 16 && fgets(logbuffer[i], sizeof(logbuffer[i]), fp) != NULL) {
                i++;
            }
            fclose(fp);
        }
        char another[1024];
        strcpy(another,intake);
        int var=strlen(another);
        another[var]='\n';
        another[var+1]='\0';

        // tokenising the input as per spaces tabs newlines and \r
        token_cnt=tokenize_input(intake);
        Command* ast=parse_tokens();     //inserting into ast
        int fl=0;
        for(Command* cmd=ast;cmd!=NULL;cmd=cmd->next){
            if(strcmp(cmd->args[0],"log")==0){
                fl=1;
                break;
            }
        }
        if(!fl){
        if(i == 0 || strcmp(another,logbuffer[i-1])!=0){
            if(i<15){
                int fd=open(logpath,O_CREAT|O_WRONLY|O_APPEND,0644);
                write(fd,another,strlen(another));
                close(fd);
            }else{
                int fd=open(logpath,O_CREAT|O_WRONLY|O_TRUNC,0644);
                for(int j=1;j<15;j++){
                    write(fd,logbuffer[j],strlen(logbuffer[j]));
                }
                write(fd,another,strlen(another));
                close(fd);
            }
        }
    }
            // write(fd,"\n",strlen("\n"));
        ////  A2 ends  /////
        

        
        //// A3 starts /////
        if(check()){
            // if(strcmp(tokens[0],"hop")==0){

            // }
        }else{
            printf("Invalid Syntax!\n");
            continue;
        }
        // Check for completed background jobs before showing prompt
        check_bg_jobs();

        // Check for activities command
        if (token_cnt == 1 && strcmp(tokens[0], "activities") == 0) {
            activities();
            continue;
        }

        // Check for ping command
        if (token_cnt == 3 && strcmp(tokens[0], "ping") == 0) {
            ping_command(tokens[1], tokens[2]);
            continue;
        }

        // fg/bg builtins
        if ((token_cnt == 1 || token_cnt == 2) && strcmp(tokens[0], "fg") == 0) {
            int have_arg = (token_cnt == 2);
            if (have_arg && !is_number_str(tokens[1])) { printf("No such job\n"); continue; }
            int job_num = have_arg ? atoi(tokens[1]) : 0;
            fg_builtin(have_arg, job_num);
            continue;
        }
        if ((token_cnt == 1 || token_cnt == 2) && strcmp(tokens[0], "bg") == 0) {
            int have_arg = (token_cnt == 2);
            if (have_arg && !is_number_str(tokens[1])) { printf("No such job\n"); continue; }
            int job_num = have_arg ? atoi(tokens[1]) : 0;
            bg_builtin(have_arg, job_num);
            continue;
        }

        // Process commands in the AST one by one
        Command* current_cmd = ast;
        while (current_cmd != NULL) {
            if (current_cmd->is_background) {
                // Fork a child for background job
                pid_t bg_pid = fork();
                if (bg_pid == 0) {
                    // Child: create its own process group for proper job control
                    setpgid(0, 0);
                    // Default signal handling in background child
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);
                    signal(SIGTTIN, SIG_DFL);
                    signal(SIGTTOU, SIG_DFL);
                    signal(SIGCHLD, SIG_DFL);
                    // Child process: redirect input to /dev/null so it can't read from terminal
                    int devnull = open("/dev/null", O_RDONLY);
                    dup2(devnull, STDIN_FILENO);
                    close(devnull);
                    // Execute only this single command
                    Command single_cmd = *current_cmd;
                    single_cmd.next = NULL;
                    execute(&single_cmd);
                    exit(0);
                } else if (bg_pid > 0) {
                    // Parent: ensure the child has its own process group
                    setpgid(bg_pid, bg_pid);
                    // Parent process: track background job info
                    if (bg_job_count < MAX_BG_JOBS) {
                        // Store job number, pid, and command name (use stable unique job numbers)
                        bg_jobs[bg_job_count].job_num = next_job_num++;
                        bg_jobs[bg_job_count].pid = bg_pid;
                        strncpy(bg_jobs[bg_job_count].cmd_name, current_cmd->args[0], 255);
                        bg_jobs[bg_job_count].cmd_name[255] = '\0';
                        // Build and store full command line for printing
                        build_cmd_line_from_args(bg_jobs[bg_job_count].cmd_line, sizeof(bg_jobs[bg_job_count].cmd_line), current_cmd->args);
                        bg_jobs[bg_job_count].active = 1;
                        bg_jobs[bg_job_count].stopped = 0;
                        // Print job info to user
                        printf("[%d] %d\n", bg_jobs[bg_job_count].job_num, bg_pid);
                        bg_job_count++;
                    }
                } else {
                    // Fork failed
                    perror("fork");
                }
                // Move to next command
                current_cmd = current_cmd->next;
            } else {
                // Foreground execution: collect all non-background commands
                Command* fg_start = current_cmd;
                Command* fg_end = current_cmd;
                
                // Find the end of consecutive foreground commands
                while (fg_end->next && !fg_end->next->is_background) {
                    fg_end = fg_end->next;
                }
                
                // Temporarily break the chain
                Command* next_after_fg = fg_end->next;
                fg_end->next = NULL;
                
                // Save command name for SIGTSTP handler
                strncpy(fg_cmd_name, fg_start->args[0], 255);
                fg_cmd_name[255] = '\0';

                // If it's a single builtin like 'hop', run in parent (no fork)
                if (fg_start == fg_end && strcmp(fg_start->name, "hop") == 0) {
                    hop(fg_start);
                    fg_end->next = next_after_fg;
                    current_cmd = next_after_fg;
                    continue;
                }

                child_pid = fork();
                if (child_pid == 0) {
                    // Set child in its own process group
                    setpgid(0, 0);
                    // Child should get default signal behavior
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);
                    signal(SIGTTIN, SIG_DFL);
                    signal(SIGTTOU, SIG_DFL);
                    signal(SIGCHLD, SIG_DFL);
                    execute(fg_start);
                    exit(0);
                } else {
                    // Set parent to wait for child in its process group
                    setpgid(child_pid, child_pid);
                    // Hand terminal control to the child's process group so it can read from TTY
                    pid_t shell_pgid = getpgrp();
                    // It's okay if tcsetpgrp fails when stdin is not a TTY (e.g., tests may pipe input)
                    if (isatty(STDIN_FILENO)) {
                        tcsetpgrp(STDIN_FILENO, child_pid);
                    }
                    int stat;
                    fg_stopped = 0;
                    fg_pid = -1;
                    // Wait for child, but detect if stopped
                    waitpid(child_pid, &stat, WUNTRACED);
                    // Take terminal control back to the shell regardless of exit/stop
            if (isatty(STDIN_FILENO)) {
                        tcsetpgrp(STDIN_FILENO, shell_pgid);
                    }
            if (fg_stopped || (WIFSTOPPED(stat))) {
                        // Move to background job list as "Stopped"
                        if (bg_job_count < MAX_BG_JOBS) {
                int assigned_job_num = next_job_num++;
                bg_jobs[bg_job_count].job_num = assigned_job_num;
                            bg_jobs[bg_job_count].pid = child_pid;
                            strncpy(bg_jobs[bg_job_count].cmd_name, fg_cmd_name, 255);
                            bg_jobs[bg_job_count].cmd_name[255] = '\0';
                            // Attempt to capture a reasonable command line from the fg_start chain's first node
                            // We don't have the full args here easily, but store the name in cmd_line as a fallback
                            memset(bg_jobs[bg_job_count].cmd_line, 0, sizeof(bg_jobs[bg_job_count].cmd_line));
                            strncpy(bg_jobs[bg_job_count].cmd_line, fg_cmd_name, sizeof(bg_jobs[bg_job_count].cmd_line) - 1);
                            bg_jobs[bg_job_count].active = 1;
                            bg_jobs[bg_job_count].stopped = 1; // Mark as stopped
                            // PRINTING PART:
        printf("\n[%d] Stopped %s\n", assigned_job_num, fg_cmd_name);
                            bg_job_count++;
                        }
                    }
                    child_pid = -1;
                }
                fg_end->next = next_after_fg;
                current_cmd = next_after_fg;
            }
        }

        }
        
    }