#include "../include/A1.h"
#include "../include/shell.h"
void computername(char* res,char* shellcwd){
    char systemname[1024];
    gethostname(systemname,sizeof(systemname));


    //generated code starts images no 1//
    int pid=getuid();
    struct passwd *pw=getpwuid(pid);

    char *username;
    if(pw != NULL){
        username=pw->pw_name;
    }
    //generated code ends//


    // char res[1024];
    strcpy(res,username);

    // char address[1024];
    // getcwd(shellcwd,sizeof(shellcwd));
    int temp=strncmp(shellcwd,home,strlen(home));


    strcat(res,"@");
    strcat(res,systemname);
    strcat(res,":");
    if(temp){
        strcat(res,shellcwd);
    }else{
        strcat(res,"~");
        strcat(res,shellcwd+strlen(home));   
    }
}