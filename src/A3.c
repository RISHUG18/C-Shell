#include "../include/A3.h"


// generated code starts//
// char** tokens;
// int token_cnt;
int currenttoken=0;
int accept(const char* t){
    if(currenttoken<token_cnt && strcmp(tokens[currenttoken],t)==0){
        return 1;
    }
    return 0;
}
int isname(const char* t){
    if(t==NULL || *t=='\0') return 0;
    for(int i=0;t[i];i++){
        if(t[i]=='|' || t[i]=='&' || t[i]=='>' || t[i]=='<' || t[i]==';'){
            return 0;
        }
    }
    return 1;
}

int name(){
    if(currenttoken<token_cnt && isname(tokens[currenttoken])){
        currenttoken++;
        return 1;
    }
    return 0;
}

int output(){
    if(accept(">") || accept(">>")){
        currenttoken++;
        return name();
    }else if(currenttoken<token_cnt && strncmp(tokens[currenttoken],">>",2)==0 && isname(tokens[currenttoken]+2)){
        currenttoken++;
        return 1;
    }else if(currenttoken<token_cnt &&  strncmp(tokens[currenttoken],">",1)==0 && isname(tokens[currenttoken]+1)){
        currenttoken++;
        return 1;
    }
    return 0;
}

int input(){
    if(accept("<")){
        currenttoken++;
        return name();
    }
    if(currenttoken<token_cnt && strncmp(tokens[currenttoken],"<",1)==0 && isname(tokens[currenttoken]+1)){
        currenttoken++;
        return 1;
    }
    return 0;
}

int atomic(){
    if(!name()){
        return 0;
    }
    while(input() || output() || name());       // it will keep increasing the currenttoken till either its neither name nor input nor output so the token at currenttoken will be neither name nor input nor output
    return 1;
}

int cmd_group(){
    if(!atomic()){
        return 0;
    }
    while(accept("|")){
        currenttoken++;
        if(!atomic()){
            return 0;
        }
    }
    return 1;
}

int shell_cmd(){
    if(!cmd_group()){
        return 0;
    }
    while(accept("&") || accept("&&") || accept(";")){
        currenttoken++;
        if(currenttoken==token_cnt){
            return 1;
        }
        if(!cmd_group()){
            return 0;
        }
    }
    return 1;
}

int check(){
    currenttoken=0;
    // tokens=tkn;
    // token_cnt=cnt;
    if(!shell_cmd()){
        return 0;
    }
    
    return currenttoken==token_cnt;
}

//generated code ends