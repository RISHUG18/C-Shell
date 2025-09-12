#include "../include/ast.h"
// #include "C2.h"
#include "B1.h"
void hop(Command* cmd){
    if(cmd->args[1]==NULL){
        getcwd(prevcwd,sizeof(prevcwd));
        if(chdir(home)==0) {
            getcwd(shellcwd, sizeof(shellcwd));
            prev_set = 1;
        }
        return;
    }
    for(int i=1;cmd->args[i]!=NULL;i++){
        if(strcmp(cmd->args[i],"~")==0){
            getcwd(prevcwd,sizeof(prevcwd));
            if(chdir(home)==0) {
                getcwd(shellcwd, sizeof(shellcwd));
                prev_set = 1;
            }
        }else if(strcmp(cmd->args[i],"..")==0){
            getcwd(prevcwd,sizeof(prevcwd));
            if(chdir("..")==0) {
                getcwd(shellcwd, sizeof(shellcwd));
                prev_set = 1;
            }
        }else if(strcmp (cmd->args[i],"-")==0){
            char temp[1024];
            strcpy(temp,prevcwd);
            getcwd(prevcwd,sizeof(prevcwd));
            if(chdir(temp)==0) {
                getcwd(shellcwd, sizeof(shellcwd));
                prev_set = 1;
            }
        }
        else{
            getcwd(prevcwd,sizeof(prevcwd));
            if(chdir(cmd->args[i])==-1){
                printf("No such directory!\n");
                continue;
            } else {
                getcwd(shellcwd, sizeof(shellcwd));
                prev_set = 1;
            }
        }
    }
}