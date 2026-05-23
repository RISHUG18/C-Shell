/*
 * reveal.c — Implementation of the reveal builtin (ls equivalent).
 *
 * Supports -a (show hidden files), -l (long/newline format).
 * Flags may be combined (-la, -al).
 * Entries are collected with readdir(), stored in a dynamic array,
 * sorted with qsort+strcmp (strict ASCII/lexicographic order), then printed.
 */

#include "../../include/builtins/reveal.h"
#include "../../include/builtins/hop.h"   /* for shell_prev_cwd, shell_prev_set */
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

/* ── qsort comparator ────────────────────────────────────────────────────── */
static int cmp_names(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

/* ── print_long_entry ────────────────────────────────────────────────────── */
static void print_long_entry(const char *dir_path, const char *name)
{
    char full[MAX_PATH * 2];
    snprintf(full, sizeof(full), "%s/%s", dir_path, name);

    struct stat st;
    if (lstat(full, &st) < 0) {
        /* If stat fails, just print the name */
        printf("%s\n", name);
        return;
    }

    /* File type and permissions */
    char perm[11];
    perm[0] = S_ISDIR(st.st_mode)  ? 'd' :
              S_ISLNK(st.st_mode)  ? 'l' :
              S_ISCHR(st.st_mode)  ? 'c' :
              S_ISBLK(st.st_mode)  ? 'b' :
              S_ISFIFO(st.st_mode) ? 'p' :
              S_ISSOCK(st.st_mode) ? 's' : '-';
    perm[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
    perm[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
    perm[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
    perm[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
    perm[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
    perm[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
    perm[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
    perm[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
    perm[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
    perm[10] = '\0';

    /* Owner and group names */
    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);
    const char *owner = pw ? pw->pw_name : "?";
    const char *group = gr ? gr->gr_name : "?";

    /* Modification time */
    char timebuf[64];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", tm_info);

    printf("%s %2ld %-8s %-8s %8lld %s %s\n",
           perm,
           (long)st.st_nlink,
           owner, group,
           (long long)st.st_size,
           timebuf,
           name);
}

/* ── list_directory ─────────────────────────────────────────────────────── */
static void list_directory(const char *path, int show_all, int long_fmt)
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        fprintf(stderr, "reveal: cannot open '%s': %s\n", path, strerror(errno));
        return;
    }

    /* Collect entries */
    char **names = NULL;
    int    count = 0, capacity = 64;
    names = malloc(capacity * sizeof(char *));
    if (names == NULL) { closedir(dir); return; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') continue;

        if (count >= capacity) {
            capacity *= 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (tmp == NULL) break;
            names = tmp;
        }
        names[count] = strdup(entry->d_name);
        if (names[count]) count++;
    }
    closedir(dir);

    /* Sort lexicographically (ASCII strcmp) */
    qsort(names, count, sizeof(char *), cmp_names);

    /* Print */
    for (int i = 0; i < count; i++) {
        if (long_fmt) {
            print_long_entry(path, names[i]);
        } else {
            printf("%s ", names[i]);
        }
        free(names[i]);
    }
    if (!long_fmt && count > 0) printf("\n");

    free(names);
}

/* ── resolve_path ────────────────────────────────────────────────────────── */
/*
 * Resolves special path tokens (~, ., .., -) into an absolute path.
 * Writes the resolved path into `out` (size `outsz`).
 * Returns 0 on success, -1 if path cannot be resolved.
 */
static int resolve_path(const char *arg, char *out, size_t outsz)
{
    if (strcmp(arg, "~") == 0) {
        strncpy(out, shell_home, outsz - 1);
        out[outsz - 1] = '\0';
    } else if (strcmp(arg, ".") == 0) {
        if (getcwd(out, outsz) == NULL) return -1;
    } else if (strcmp(arg, "..") == 0) {
        char save[MAX_PATH];
        if (getcwd(save, sizeof(save)) == NULL) return -1;
        if (chdir("..") != 0) return -1;
        if (getcwd(out, outsz) == NULL) { chdir(save); return -1; }
        chdir(save);
    } else if (strcmp(arg, "-") == 0) {
        if (!shell_prev_set) { fprintf(stderr, "reveal: no previous directory\n"); return -1; }
        strncpy(out, shell_prev_cwd, outsz - 1);
        out[outsz - 1] = '\0';
    } else {
        strncpy(out, arg, outsz - 1);
        out[outsz - 1] = '\0';
    }
    return 0;
}

/* ── builtin_reveal ─────────────────────────────────────────────────────── */
void builtin_reveal(Command *cmd)
{
    int show_all = 0, long_fmt = 0;
    char *path_arg = NULL;

    /* Parse flags and path */
    for (int i = 1; i < cmd->argc; i++) {
        char *tok = cmd->argv[i];
        if (tok[0] == '-' && tok[1] != '\0') {
            for (int j = 1; tok[j] != '\0'; j++) {
                if      (tok[j] == 'a') show_all = 1;
                else if (tok[j] == 'l') long_fmt = 1;
                /* unknown flags silently ignored per spec */
            }
        } else {
            if (path_arg == NULL) {
                path_arg = tok;
            } else {
                fprintf(stderr, "reveal: too many arguments\n");
                return;
            }
        }
    }

    /* Resolve target path */
    char target[MAX_PATH];
    if (path_arg == NULL) {
        if (getcwd(target, sizeof(target)) == NULL) {
            perror("reveal: getcwd");
            return;
        }
    } else {
        if (resolve_path(path_arg, target, sizeof(target)) < 0) return;
    }

    list_directory(target, show_all, long_fmt);
}
