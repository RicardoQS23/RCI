//#include "project.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

enum commands{
    JOIN,
    DJOIN,
    CREATE,
    DELETE,
    GET,
    LEAVE,
    EXIT
};

int compare_cmd(char *line_cmd)
{
    char cmds[7][7] = {"join", "djoin", "create", "delete", "get", "leave", "exit"};
    for (int i = 0; i < 7; i++)
    {
        if(strcmp(line_cmd,cmds[i]) == 0)
            return i;
    }
    return -1;
}

int main(int argc, char *argv[])
{
    enum commands cmd;
    int cmd_code = 0;
    //char net[4], id[3], bootid[3], bootIP[15], bootTCP[6]; 
    /* Command line treatment*/
    cmd_code = compare_cmd(argv[1]);
    switch(cmd_code){
        case 0:
            cmd = JOIN;
            break;
        case 1:
            cmd = DJOIN;
            break;
        case 2:
            cmd = CREATE;
            break;
        case 3:
            cmd = DELETE;
            break;
        case 4:
            cmd = GET;
            break;
        case 5:
            cmd = LEAVE;
            break;
        case 6:
            cmd = EXIT;
            break;
        default:
            printf("Unknown Command!\n");
            exit(1);
    }
    printf("ola");
    return 0;
}
