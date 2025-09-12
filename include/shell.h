#include "tokenize.h"
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pwd.h>
#include<signal.h>
#include <fcntl.h>
#define MAX_TOKENS 1000
#define MAX_LENGTH 1000

extern char *tokens[MAX_TOKENS];
extern int token_cnt;
extern char home[1024];
extern char shellcwd[1024];
extern char prevcwd[1024];
extern int prev_set; // 0 until a successful hop occurs