#include "../../include/builtins/reveal.h"
#include "../../include/builtins/hop.h"
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

static int cmp_names(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

static void print_long(const char *dir_path, const char *name)
{
    char full[MAX_PATH * 2];
    snprintf(full, sizeof(full), "%s/%s", dir_path, name);

    struct stat st;
    if (lstat(full, &st) < 0) { printf("%s\n", name); return; }

    char p[11];
    p[0]  = S_ISDIR(st.st_mode)  ? 'd' : S_ISLNK(st.st_mode) ? 'l' :
            S_ISCHR(st.st_mode)  ? 'c' : S_ISBLK(st.st_mode) ? 'b' :
            S_ISFIFO(st.st_mode) ? 'p' : S_ISSOCK(st.st_mode) ? 's' : '-';
    p[1]  = (st.st_mode & S_IRUSR) ? 'r' : '-';
    p[2]  = (st.st_mode & S_IWUSR) ? 'w' : '-';
    p[3]  = (st.st_mode & S_IXUSR) ? 'x' : '-';
    p[4]  = (st.st_mode & S_IRGRP) ? 'r' : '-';
    p[5]  = (st.st_mode & S_IWGRP) ? 'w' : '-';
    p[6]  = (st.st_mode & S_IXGRP) ? 'x' : '-';
    p[7]  = (st.st_mode & S_IROTH) ? 'r' : '-';
    p[8]  = (st.st_mode & S_IWOTH) ? 'w' : '-';
    p[9]  = (st.st_mode & S_IXOTH) ? 'x' : '-';
    p[10] = '\0';

    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);
    char tbuf[64];
    struct tm *tm = localtime(&st.st_mtime);
    strftime(tbuf, sizeof(tbuf), "%b %e %H:%M", tm);

    printf("%s %2ld %-8s %-8s %8lld %s %s\n",
           p, (long)st.st_nlink,
           pw ? pw->pw_name : "?", gr ? gr->gr_name : "?",
           (long long)st.st_size, tbuf, name);
}

static void list_dir(const char *path, int show_all, int long_fmt)
{
    DIR *dir = opendir(path);
    if (!dir) { fprintf(stderr, "reveal: cannot open '%s': %s\n", path, strerror(errno)); return; }

    char **names  = NULL;
    int    count  = 0, cap = 64;
    names = malloc(cap * sizeof(char *));
    if (!names) { closedir(dir); return; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') continue;
        if (count >= cap) {
            cap *= 2;
            char **tmp = realloc(names, cap * sizeof(char *));
            if (!tmp) break;
            names = tmp;
        }
        names[count] = strdup(entry->d_name);
        if (names[count]) count++;
    }
    closedir(dir);

    qsort(names, count, sizeof(char *), cmp_names);

    for (int i = 0; i < count; i++) {
        if (long_fmt) print_long(path, names[i]);
        else          printf("%s ", names[i]);
        free(names[i]);
    }
    if (!long_fmt && count > 0) printf("\n");
    free(names);
}

static int resolve_path(const char *arg, char *out, size_t sz)
{
    if (strcmp(arg, "~") == 0) {
        strncpy(out, shell_home, sz - 1); out[sz - 1] = '\0';
    } else if (strcmp(arg, ".") == 0) {
        if (!getcwd(out, sz)) return -1;
    } else if (strcmp(arg, "..") == 0) {
        char save[MAX_PATH];
        if (!getcwd(save, sizeof(save))) return -1;
        if (chdir("..") != 0) return -1;
        if (!getcwd(out, sz)) { chdir(save); return -1; }
        chdir(save);
    } else if (strcmp(arg, "-") == 0) {
        if (!shell_prev_set) { fprintf(stderr, "reveal: no previous directory\n"); return -1; }
        strncpy(out, shell_prev_cwd, sz - 1); out[sz - 1] = '\0';
    } else {
        strncpy(out, arg, sz - 1); out[sz - 1] = '\0';
    }
    return 0;
}

void builtin_reveal(Command *cmd)
{
    int show_all = 0, long_fmt = 0;
    char *path_arg = NULL;

    for (int i = 1; i < cmd->argc; i++) {
        char *tok = cmd->argv[i];
        if (tok[0] == '-' && tok[1]) {
            for (int j = 1; tok[j]; j++) {
                if      (tok[j] == 'a') show_all = 1;
                else if (tok[j] == 'l') long_fmt = 1;
            }
        } else {
            if (!path_arg) path_arg = tok;
            else { fprintf(stderr, "reveal: too many arguments\n"); return; }
        }
    }

    char target[MAX_PATH];
    if (!path_arg) {
        if (!getcwd(target, sizeof(target))) { perror("reveal: getcwd"); return; }
    } else {
        if (resolve_path(path_arg, target, sizeof(target)) < 0) return;
    }

    list_dir(target, show_all, long_fmt);
}
