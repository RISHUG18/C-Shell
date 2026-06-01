#include "../include/prompt.h"

void get_display_cwd(char *buf, size_t bufsz)
{
    char cwd[MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strncpy(cwd, shell_cwd, sizeof(cwd) - 1);
        cwd[sizeof(cwd) - 1] = '\0';
    }

    strncpy(shell_cwd, cwd, MAX_PATH - 1);
    shell_cwd[MAX_PATH - 1] = '\0';

    size_t hlen = strlen(shell_home);
    if (hlen > 0 &&
        strncmp(cwd, shell_home, hlen) == 0 &&
        (cwd[hlen] == '\0' || cwd[hlen] == '/'))
    {
        if (cwd[hlen] == '\0')
            snprintf(buf, bufsz, "~");
        else
            snprintf(buf, bufsz, "~%s", cwd + hlen);
    } else {
        snprintf(buf, bufsz, "%s", cwd);
    }
}

void print_prompt(void)
{
    char username[MAX_USERNAME];
    char hostname[MAX_HOSTNAME];
    char disp_cwd[MAX_PATH];

    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (pw)
        strncpy(username, pw->pw_name, sizeof(username) - 1);
    else
        snprintf(username, sizeof(username), "%u", (unsigned)uid);
    username[sizeof(username) - 1] = '\0';

    if (gethostname(hostname, sizeof(hostname)) != 0)
        strncpy(hostname, "localhost", sizeof(hostname) - 1);
    hostname[sizeof(hostname) - 1] = '\0';

    get_display_cwd(disp_cwd, sizeof(disp_cwd));

    printf("<%s@%s:%s> ", username, hostname, disp_cwd);
    fflush(stdout);
}
