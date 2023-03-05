//#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>

#define MAX_BUFFER_SIZE 516
#define max(A, B) ((A)>=(B)? (A):(B))

enum commands{
    JOIN,
    DJOIN,
    CREATE,
    DELETE,
    GET,
    LEAVE,
    EXIT
};
int compare_cmd(char *cmdLine);
void udpClient(char *buffer, char *ip, char *port, char *net);
void handleCommandLine(char **cmdLine);
int validate_number(char *str);
int validate_ip(char *ip);

int main(int argc, char *argv[])
{

    enum commands cmd;
    int cmd_code = 0, counter, tk_counter;
    char machineIP[16], tcpPort[6], regIP[16], regUDP[6];
    char buffer[MAX_BUFFER_SIZE], *token;

    fd_set rfds, inputs;
    int newfd;
    struct timeval timeout;
    char net[4], id[3], bootid[3], bootIP[16], bootTCP[6], name[MAX_BUFFER_SIZE], dest[3]; 
    /* Command line treatment*/
    strcpy(machineIP, argv[1]);
    strcpy(tcpPort, argv[2]);
    strcpy(regIP, argv[3]);
    strcpy(regUDP, argv[4]);

    //handleCommandLine(argv);
    printf("Size of fd_set: %ld\n",sizeof(fd_set));
    printf("Value of FD_SETSIZE: %d\n",FD_SETSIZE);
    
    while(1)
    {
        FD_SET(newfd, &inputs);
        rfds=inputs;

        memset((void *)&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 10;

        counter = select(newfd + 1, &rfds, (fd_set *) NULL, (fd_set *) NULL, (struct timeval *) &timeout);
        switch(counter)
        {
            case 0:
                printf("running ...\n");
                break;

            case -1:
                perror("select");
                exit(1);

            default: 
                for(;counter;){

                    if(FD_ISSET(0, &rfds))
                    {   
                        fgets(buffer, sizeof(buffer), stdin);
                        //if(parseUserInput(buffer, &self_node) == 0)printf("Invalid Input\n");
                        counter--;
                        token = strtok(buffer, " ");
                        cmd_code = compare_cmd(token);
                        switch(cmd_code){
                            case 0:
                                cmd = JOIN;
                                tk_counter = 0;
                                while(token != NULL)
                                {
                                    token = strtok(NULL," ");
                                    switch(tk_counter)
                                    {
                                        case 0:
                                            strcpy(net,token);
                                            break;
                                        case 1:
                                            strcpy(id,token);
                                            break;
                                        default:
                                            break;
                                    }
                                    tk_counter++;
                                }
                                break;
                            case 1:
                                cmd = DJOIN;
                                tk_counter = 0;
                                while(token != NULL)
                                {
                                    token = strtok(NULL," ");
                                    switch(tk_counter)
                                    {
                                        case 0:
                                            strcpy(net,token);
                                            break;
                                        case 1:
                                            strcpy(id,token);
                                            break;
                                        case 2:
                                            strcpy(bootid,token);
                                            break;
                                        case 3:
                                            strcpy(bootIP,token);
                                            break;
                                        case 4:
                                            strcpy(bootTCP,token);
                                            break;
                                        default:
                                            break;
                                    }
                                    tk_counter++;
                                }
                                break;
                            case 2:
                                cmd = CREATE;
                                token = strtok(NULL," ");
                                strcpy(name,token);
                                break;
                            case 3:
                                cmd = DELETE;
                                token = strtok(NULL," ");
                                strcpy(name,token);
                                break;
                            case 4:
                                cmd = GET;
                                token = strtok(NULL," ");
                                switch(tk_counter)
                                    {
                                        case 0:
                                            strcpy(dest,token);
                                            break;
                                        case 1:
                                            strcpy(name,token);
                                            break;
                                        default:
                                            break;
                                    }
                                tk_counter++;
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
                        switch(cmd){
                            case JOIN:
                                //DO STUFF ACCORDING TO THE COMMAND
                                udpClient(buffer, regIP, regUDP, net);
                                break;
                            case DJOIN:
                                //DO STUFF ACCORDING TO THE COMMAND
                                break;
                            case CREATE:
                                //DO STUFF ACCORDING TO THE COMMAND
                                break;
                            case DELETE:
                                //DO STUFF ACCORDING TO THE COMMAND
                                break;
                            case GET:
                                //DO STUFF ACCORDING TO THE COMMAND
                                break;
                            case EXIT:
                                //DO STUFF ACCORDING TO THE COMMAND
                                break;
                        }
                    }
                    /*else if(FD_ISSET(serverSocket,&rfds)){
                        newfd = greetNewNeighbour(serverSocket, &self_node);
                        counter--;
                    }
                    else if (FD_ISSET(self_node.exterior.socket, &rfds))
                    {
                        counter--;
                        //Handle exterior neighbour message
                    }
                    else{

                        for(NODE *neighbour = self_node.neighbourList; neighbour != NULL; neighbour = neighbour->next)
                        {
                            if(FD_ISSET(neighbour->socket, &rfds))
                            {
                                counter--;
                                //handle interior neighbour message
                            }
                        }
                    }

                    */
                }
        }
    } 
    return 0;
}


void udpClient(char *buffer, char *ip, char *port, char *net)
{   
    int errcode, fd;
    ssize_t n;
    struct addrinfo hints, *res;
    socklen_t addrlen;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd == -1)    exit(1);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    errcode = getaddrinfo(ip, port, &hints, &res);
    if(errcode != 0)    exit(1);

    sprintf(buffer, "NODES %s", net);
    n = sendto(fd, buffer,strlen(buffer)+1, 0, res->ai_addr, res->ai_addrlen);
    if(n == -1)   exit(1);

    addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr*) &addr, &addrlen);
    if(n == -1) exit(1);
    buffer[n] = '\0';

    write(1, "echo: ", 6); write(1, buffer, n);
    freeaddrinfo(res);
    close(fd);
}

int compare_cmd(char *cmdLine)
{
    char cmds[7][7] = {"join", "djoin", "create", "delete", "get", "leave", "exit"};
    for (int i = 0; i < 7; i++)
    {
        if(strcmp(cmdLine,cmds[i]) == 0)
            return i;
    }
    return -1;
}

void handleCommandLine(char **cmdLine)
{
    // Check machineIP
    if(validate_ip(cmdLine[1]) == 0)
        exit(1);

    // Check TCP
    if(strlen(cmdLine[2]) > 6) {
        exit(1);
    }

    // Check regIP
    if(validate_ip(cmdLine[4]) == 0)
        exit(1);

     // Check regUDP
    if(strlen(cmdLine[2]) > 6) {
        exit(1);
    }
}

int validate_number(char *str) {
    while (*str != '\0') {
      if(!isdigit(*str))    return 0; //if the character is not a number, return false
      str++; //point to next character
    }
    return 1;
}

int validate_ip(char *ip) { //check whether the IP is valid or not
    int i, num, dots = 0;
    char *ptr;
    if (ip == NULL) return 0;
    ptr = strtok(ip, "."); //cut the string using dor delimiter
    if (ptr == NULL)    return 0;
            
    while (ptr) 
    {
        if (!validate_number(ptr))    return 0; //check whether the sub string is  holding only number or not          
        num = atoi(ptr); //convert substring to number
        if (num >= 0 && num <= 255)
        {
            ptr = strtok(NULL, "."); //cut the next part of the string
            if (ptr != NULL)    dots++; //increase the dot count
        } else
            return 0;
    }
    if (dots != 3)  return 0; //if the number of dots are not 3, return false
        
    return 1;
}

/*int main(void)
{
    struct addrinfo hints,*res;
    int fd, errcode, sendbytes;
    ssize_t n;
    char buffer[MAXLINE];

    fd=socket(AF_INET,SOCK_STREAM,0);//UDP socket
    if(fd==-1)exit(1);

    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET;//IPv4
    hints.ai_socktype=SOCK_STREAM;//UDP socket

    errcode=getaddrinfo("google.com",PORT,&hints,&res);
    if(errcode!=0)exit(1);

    n=connect(fd,res->ai_addr, res->ai_addrlen);
    if(n==-1) exit(1);

    sprintf(buffer,"GET / HTTP/1.1\r\n\r\n");
    sendbytes = strlen(buffer);

    if (write(fd, buffer, sendbytes) != sendbytes)  exit(1);

    while((n = read(fd, buffer, MAXLINE-1)) > 0)
    {
        write(1, "echo: ", 6); write(1,buffer,n);
    }
    if(n==-1)exit(1);

    freeaddrinfo(res);
    close(fd);
    exit(0);
};
*/

/*
int main(void)
{
    char buffer[128];

    fd_set inputs, testfds;
    struct timeval timeout;

    int i,out_fds,n,errcode;

    FD_ZERO(&inputs); // Clear inputs
    FD_SET(0,&inputs); // Set standard input channel on
    printf("Size of fd_set: %ld\n",sizeof(fd_set));
    printf("Value of FD_SETSIZE: %d\n",FD_SETSIZE);
    while(1)
    {
        testfds=inputs;
//        printf("testfds byte: %d\n",((char *)&testfds)[0]);
        memset((void *)&timeout,0,sizeof(timeout));
        timeout.tv_sec=3;

        out_fds=select(FD_SETSIZE,&testfds,(fd_set *)NULL,(fd_set *)NULL,(struct timeval *) &timeout);

        switch(out_fds)
        {
            case 0:
                printf("\n ---------------Timeout event-----------------\n");
                break;
            case -1:
                perror("select");
                exit(1);
            default:
                if(FD_ISSET(0,&testfds))
                {
                    gets(buffer);
                    printf("------------------------------Input at keyboard: %s\n",buffer);
                }
        }

    }

}
*/
