/*
 * log.c — Persistent command history for C-Shell.
 *
 * Stores up to LOG_MAX_ENTRIES lines in LOG_FILE.
 * Ignores commands that contain "log" or are consecutive duplicates.
 */

#include "../../include/builtins/log.h"
#include "../../include/execute.h"   /* for execute_command_group_list */
#include "../../include/lexer.h"
#include "../../include/parse.h"

/* ── load_log ───────────────────────────────────────────────────────────── */
/*
 * Read all entries from LOG_FILE into buf (array of strings).
 * Returns the count of entries read (0 if file doesn't exist).
 * Caller must free each entry and the array.
 */
static int load_log(char lines[LOG_MAX_ENTRIES][MAX_INPUT])
{
    FILE *fp = fopen(LOG_FILE, "r");
    if (fp == NULL) return 0;
    int n = 0;
    while (n < LOG_MAX_ENTRIES && fgets(lines[n], MAX_INPUT, fp) != NULL) {
        /* Strip trailing newline */
        lines[n][strcspn(lines[n], "\n")] = '\0';
        if (lines[n][0] != '\0') n++;
    }
    fclose(fp);
    return n;
}

/* ── save_log ───────────────────────────────────────────────────────────── */
static void save_log(char lines[LOG_MAX_ENTRIES][MAX_INPUT], int n)
{
    FILE *fp = fopen(LOG_FILE, "w");
    if (fp == NULL) { perror("log: fopen"); return; }
    for (int i = 0; i < n; i++) fprintf(fp, "%s\n", lines[i]);
    fclose(fp);
}

/* ── log_record ─────────────────────────────────────────────────────────── */
void log_record(const char *input_line)
{
    if (input_line == NULL || input_line[0] == '\0') return;

    /* Skip if line contains "log" as a word */
    if (strstr(input_line, "log") != NULL) return;

    char lines[LOG_MAX_ENTRIES][MAX_INPUT];
    int n = load_log(lines);

    /* Skip consecutive duplicate */
    if (n > 0 && strcmp(lines[n - 1], input_line) == 0) return;

    if (n < LOG_MAX_ENTRIES) {
        /* Room available: append */
        strncpy(lines[n], input_line, MAX_INPUT - 1);
        lines[n][MAX_INPUT - 1] = '\0';
        n++;
    } else {
        /* Full: drop oldest, shift left, append new */
        for (int i = 0; i < LOG_MAX_ENTRIES - 1; i++) {
            strncpy(lines[i], lines[i + 1], MAX_INPUT - 1);
        }
        strncpy(lines[LOG_MAX_ENTRIES - 1], input_line, MAX_INPUT - 1);
        lines[LOG_MAX_ENTRIES - 1][MAX_INPUT - 1] = '\0';
    }

    save_log(lines, n);
}

/* ── builtin_log ─────────────────────────────────────────────────────────── */
void builtin_log(Command *cmd)
{
    char lines[LOG_MAX_ENTRIES][MAX_INPUT];
    int n = load_log(lines);

    /* log (no args): print history, oldest first */
    if (cmd->argc == 1) {
        for (int i = 0; i < n; i++) printf("%s\n", lines[i]);
        return;
    }

    /* log purge */
    if (strcmp(cmd->argv[1], "purge") == 0) {
        FILE *fp = fopen(LOG_FILE, "w");
        if (fp) fclose(fp);
        return;
    }

    /* log execute <N> */
    if (strcmp(cmd->argv[1], "execute") == 0) {
        if (cmd->argc < 3) {
            fprintf(stderr, "log execute: missing index\n");
            return;
        }
        int idx = atoi(cmd->argv[2]);
        if (idx < 1 || idx > n) {
            fprintf(stderr, "log execute: index %d out of range (1..%d)\n", idx, n);
            return;
        }
        /* Index 1 = most recent = lines[n-1] */
        const char *to_run = lines[n - idx];
        printf("%s\n", to_run);

        /* Tokenise and execute the retrieved command */
        char buf[MAX_INPUT];
        strncpy(buf, to_run, MAX_INPUT - 1);
        buf[MAX_INPUT - 1] = '\0';

        TokenList *tl = lexer_tokenise(buf);
        if (tl == NULL) return;
        CommandGroup *cg = parse(tl);
        free_token_list(tl);
        if (cg == NULL) return;
        execute_command_group_list(cg);
        free_command_group_list(cg);
        return;
    }

    fprintf(stderr, "log: unknown subcommand '%s'\n", cmd->argv[1]);
}
