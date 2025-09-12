#include "../include/ast.h"
#include "../include/B2.h"
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool l = false;
bool a = false;

// Comparator for qsort
int cmpfunc(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

void print_dir(DIR* dir, bool show_all, bool newline) {
    struct dirent* entry;
    char* files[1024];  // Max 1024 files (you can malloc dynamically if needed)
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') {
            continue;  // skip hidden files if `-a` not set
        }
        files[count] = strdup(entry->d_name); // copy filename
        count++;
    }

    // Sort filenames
    qsort(files, count, sizeof(char*), cmpfunc);

    // Print filenames
    for (int i = 0; i < count; i++) {
        if (newline)
            printf("%s\n", files[i]);
        else
            printf("%s ", files[i]);

        free(files[i]); // free strdup
    }
    if (!newline) printf("\n");
}

void la(DIR* dir) { print_dir(dir, true, true); }
void onlyl(DIR* dir) { print_dir(dir, false, true); }
void onlya(DIR* dir) { print_dir(dir, true, false); }
void def(DIR* dir) { print_dir(dir, false, false); }

void tocall(char* var) {
    DIR* dir = opendir(var);
    if (!dir) {
        printf("No such directory!\n");
        return;
    }
    if (a && l) {
        la(dir);
    } else if (a) {
        onlya(dir);
    } else if (l) {
        onlyl(dir);
    } else {
        def(dir);
    }
    closedir(dir);
}

void reveal(Command* cmd) {
    a = false;
    l = false;
    // Parse flags and at most one path argument
    char* path_arg = NULL;
    for (int i = 1; cmd->args[i] != NULL; i++) {
        char* tok = cmd->args[i];
        if (tok[0] == '-' && tok[1] != '\0') {
            // flags may be combined like -la or -al
            for (int j = 1; tok[j] != '\0'; j++) {
                if (tok[j] == 'a') a = true;
                else if (tok[j] == 'l') l = true;
                else { /* ignore unknown flags per spec not required */ }
            }
        } else if (path_arg == NULL) {
            path_arg = tok;
        } else {
            // more than one path-like token -> invalid syntax
            printf("reveal: Invalid Syntax!\n");
            return;
        }
    }

    // Resolve the target directory similar to hop's semantics
    char target[1024];
    if (path_arg == NULL) {
        // default: current working directory
        getcwd(target, sizeof(target));
        tocall(target);
        return;
    }

    if (strcmp(path_arg, "~") == 0) {
        tocall(home);
        return;
    }
    if (strcmp(path_arg, ".") == 0) {
        getcwd(target, sizeof(target));
        tocall(target);
        return;
    }
    if (strcmp(path_arg, "..") == 0) {
        char save[1024];
        getcwd(save, sizeof(save));
        if (chdir("..") == 0) {
            getcwd(target, sizeof(target));
            chdir(save);
            tocall(target);
        } else {
            printf("No such directory!\n");
        }
        return;
    }
    if (strcmp(path_arg, "-") == 0) {
        if (!prev_set) {
            printf("No such directory!\n");
            return;
        }
        tocall(prevcwd);
        return;
    }
    // Otherwise, treat as a directory name or path
    tocall(path_arg);
}
