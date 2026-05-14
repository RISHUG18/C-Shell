/*
 * prompt.c — Shell prompt implementation.
 *
 * Implements <username@hostname:cwd> prompt display.
 * Uses POSIX APIs only:  getuid(), getpwuid(), gethostname(), getcwd().
 *
 * Home-directory substitution:
 *   If shell_home is a prefix of the current working directory, the
 *   matching leading path segment is replaced with '~'.
 *   e.g., /home/user/project  →  ~/project
 *         /tmp/work            →  /tmp/work   (no substitution)
 */

#include "../include/prompt.h"

/* ── get_display_cwd ──────────────────────────────────────────────────────── */
void get_display_cwd(char *buf, size_t bufsz)
{
    char cwd[MAX_PATH];

    /* Refresh cwd from the OS every time we print the prompt */
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        /* Fallback: use the stored shell_cwd */
        strncpy(cwd, shell_cwd, sizeof(cwd) - 1);
        cwd[sizeof(cwd) - 1] = '\0';
    }

    /* Update the shared shell_cwd global */
    strncpy(shell_cwd, cwd, MAX_PATH - 1);
    shell_cwd[MAX_PATH - 1] = '\0';

    size_t home_len = strlen(shell_home);

    /*
     * Check whether shell_home is a proper prefix of (or equal to) cwd.
     * We require:
     *   1. cwd starts with shell_home  (strncmp == 0)
     *   2. The character immediately after the home prefix in cwd is either
     *      '\0' (cwd == home) or '/' (cwd is inside home), preventing
     *      false matches like home=/home/alice matching /home/alice_twin.
     */
    if (home_len > 0 &&
        strncmp(cwd, shell_home, home_len) == 0 &&
        (cwd[home_len] == '\0' || cwd[home_len] == '/'))
    {
        /* Build "~" + remainder */
        if (cwd[home_len] == '\0') {
            /* cwd is exactly the home directory */
            snprintf(buf, bufsz, "~");
        } else {
            /* cwd is a subdirectory of home */
            snprintf(buf, bufsz, "~%s", cwd + home_len);
        }
    } else {
        /* Not under home — print absolute path */
        snprintf(buf, bufsz, "%s", cwd);
    }
}

/* ── print_prompt ─────────────────────────────────────────────────────────── */
void print_prompt(void)
{
    char username[MAX_USERNAME];
    char hostname[MAX_HOSTNAME];
    char disp_cwd[MAX_PATH];

    /* ── Username via POSIX passwd entry ── */
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (pw != NULL) {
        strncpy(username, pw->pw_name, sizeof(username) - 1);
    } else {
        /* Fallback: numeric UID as string */
        snprintf(username, sizeof(username), "%u", (unsigned)uid);
    }
    username[sizeof(username) - 1] = '\0';

    /* ── Hostname ── */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "localhost", sizeof(hostname) - 1);
    }
    hostname[sizeof(hostname) - 1] = '\0';

    /* ── Current working directory (with ~ substitution) ── */
    get_display_cwd(disp_cwd, sizeof(disp_cwd));

    /* ── Print prompt: <user@host:cwd>  ── */
    printf("<%s@%s:%s> ", username, hostname, disp_cwd);
    fflush(stdout);
}
