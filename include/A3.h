#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pwd.h>
// #include "tokenize.h"
#include "shell.h"

int isname(const char* t);
int name();
int input();
int output();
int atomic();
int cmd_group();
int shell_cmd();
int accept(const char *t);
int check();
