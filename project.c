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

#define MAX_BUFFER_SIZE 2064
#define max(A, B) ((A)>=(B)? (A):(B))


typedef struct NODE{
    int fd;
    char ip[16];
    char port[6];
    char id[3];
}NODE;

typedef struct AppNode{
    NODE self;
    NODE bck;
    NODE ext;
    NODE intr[99];
}AppNode;

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
int connectTcpClient(AppNode *app);
void connectUdpClient(char *buffer, char *ip, char *port, char *net, AppNode *app);
void handleCommandLine(char **cmdLine);
int validate_number(char *str);
int validate_ip(char *ip);

int main(int argc, char *argv[])
{
    AppNode app;
    char regIP[16] = "193.136.138.142", regUDP[6] = "59000";
    char buffer[MAX_BUFFER_SIZE], net[4];
    fd_set readSockets, currentSockets;
    int newfd, counter;
    struct timeval timeout;
    enum commands cmd;

    strcpy(app.self.ip, "192.168.1.74");
    strcpy(app.self.port, "59000");

    FD_ZERO(&currentSockets);    
    FD_SET(0,&currentSockets);  

    while(1)
    {
        readSockets = currentSockets;
        memset((void *)&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 10;

        counter = select(FD_SETSIZE, &readSockets, (fd_set *) NULL, (fd_set *) NULL, (struct timeval *) &timeout);

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
                    if(FD_ISSET(0, &readSockets))
                    {   
                        fgets(buffer, sizeof(buffer), stdin);
                        counter--;
                        cmd = JOIN;
                        strcpy(net, "088");
                        strcpy(app.self.id, "10");
                        switch(cmd){
                            case JOIN:
                                //Nodes's initialization
                                memmove(&app.ext, &app.self, sizeof(NODE));
                                memmove(&app.bck, &app.self, sizeof(NODE));
                                sprintf(buffer, "NODES %s", net);
                                udpClient(buffer, regIP, regUDP, net);  //gets net nodes
                                if(sscanf(buffer, "%*s %*s %s %s %s", app.ext.id, app.ext.ip, app.ext.port) == 3)
                                {
                                    memmove(&app.bck, &app.ext, sizeof(NODE));
                                    if((app.ext.fd = connectTcpClient(&app)) > 0)
                                    {
                                        FD_SET(app.ext.fd, &currentSockets);  //sucessfully connected to ext node
                                        printf("Connected to %s\n", app.ext.id);
                                    }
                                    else
                                    {
                                        printf("TCP ERROR!!\n");
                                        memmove(&app.ext, &app.self, sizeof(NODE)); //couldnt connect to ext node
                                    }   
                                }
                                else
                                {
                                    //rede vazia
                                    connectUdpClient(buffer, regIP, regUDP, net, &app);

                                }
                                break;
                            case LEAVE:
                                sprintf(buffer, "UNREG %s %s\n", net, app.self.id);
                                udpClient(buffer, regIP, regUDP, net);
                                break;
                            default:
                                printf("default\n");
                                break;
                        }
                    }
                }
        }
    }
                    //recebo pedido de ligaçao atraves do listen
                    /*else if(FD_ISSET(serverSocket,&readtSockets)){
                        newfd = greetNewNeighbour(serverSocket, &self_node);
                        counter--;
                    }
                    else if (FD_ISSET(self_node.exterior.socket, &readtSockets))
                    {
                        counter--;
                        //Handle exterior neighbour message
                    }
                    else{

                        for(NODE *neighbour = self_node.neighbourList; neighbour != NULL; neighbour = neighbour->next)
                        {
                            if(FD_ISSET(neighbour->socket, &readtSockets))
                            {
                                counter--;
                                //handle interior neighbour message
                            }
                        }
                    }

                    
                }

        
    }



    /*AppNode app;
    enum commands cmd;
    int cmd_code = 0, counter, tk_counter;
    char regIP[16], regUDP[6];
    char buffer[MAX_BUFFER_SIZE], *token;

    fd_set readSockets, currentSockets;
    int newfd, clientFd;
    struct timeval timeout;
    char net[4], id[3], bootid[3], bootIP[16], bootTCP[6], name[MAX_BUFFER_SIZE], dest[3]; 
    // Command line treatment

    //handleCommandLine(argv);
    FD_ZERO(&currentSockets);  
    FD_SET(0,&currentSockets);  
    while(1)
    {
        readSockets = currentSockets;
        memset((void *)&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 15;

        counter = select(FD_SETSIZE, &readSockets, (fd_set *) NULL, (fd_set *) NULL, (struct timeval *) &timeout);
        printf("pixa\n");
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

                    if(FD_ISSET(0, &readSockets))
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
                                            strcpy(app.self.id,token);
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
                                //Nodes's initialization

                                memmove(&app.ext, &app.self, sizeof(NODE));
                                memmove(&app.bck, &app.self, sizeof(NODE));

                                udpClient(buffer, regIP, regUDP, net);  //gets net nodes
                                printf("hey!\n");
                                if(sscanf(buffer, "%*s %*s %s %s %s", app.ext.id, app.ext.ip, app.ext.port) == 3)
                                {
                                    memmove(&app.bck, &app.ext, sizeof(NODE));
                                    if((clientFd = connectTcpClient(&app)) > 0)
                                    {
                                        FD_SET(clientFd, &currentSockets);  //sucessfully connected to ext node
                                        printf("Connected to %s\n", app.ext.id);
                                    }
                                    else
                                    {
                                        printf("HERE!!\n");
                                        memmove(&app.ext, &app.self, sizeof(NODE)); //couldnt connect to ext node
                                    }   
                                }
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
                            default:
                                printf("dadsadad\n");
                                break;
                        }
                    }
                    //recebo pedido de ligaçao atraves do listen
                    /*else if(FD_ISSET(serverSocket,&readtSockets)){
                        newfd = greetNewNeighbour(serverSocket, &self_node);
                        counter--;
                    }
                    else if (FD_ISSET(self_node.exterior.socket, &readtSockets))
                    {
                        counter--;
                        //Handle exterior neighbour message
                    }
                    else{

                        for(NODE *neighbour = self_node.neighbourList; neighbour != NULL; neighbour = neighbour->next)
                        {
                            if(FD_ISSET(neighbour->socket, &readtSockets))
                            {
                                counter--;
                                //handle interior neighbour message
                            }
                        }
                    }

                    
                }
        }
    } 
    */
    return 0;
}

int connectTcpClient(AppNode *app)
{
    int fd;
    struct addrinfo hints, *res;
    char buffer[516];

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1)    return -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    printf("%s %s\n",app->ext.ip, app->ext.port);
    if(getaddrinfo(app->ext.ip, app->ext.port, &hints, &res) != 0);
    {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    if(connect(fd, res->ai_addr, res->ai_addrlen) < 0)
    {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    sprintf(buffer, "NEW %s %s %s\n", app->self.id, app->self.ip, app->self.port);
    if(write(fd, buffer, strlen(buffer)) < 0)
    {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    return fd;
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

    //sprintf(buffer, "NODES %s", net);
    n = sendto(fd, buffer,strlen(buffer)+1, 0, res->ai_addr, res->ai_addrlen);
    if(n == -1)     exit(1);

    addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr*) &addr, &addrlen);
    if(n == -1)    exit(1);
    buffer[n] = '\0';

    write(1, buffer, n);
    freeaddrinfo(res);
    close(fd);
}

void connectUdpClient(char *buffer, char *ip, char *port, char *net, AppNode *app)
{   
    int errcode, fd;
    ssize_t n;
    struct addrinfo hints, *res;
    socklen_t addrlen;
    struct sockaddr_in addr;
    printf("connect\n");

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd == -1)    exit(1);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;


    errcode = getaddrinfo(ip, port, &hints, &res);
    if(errcode != 0)
    {
        printf("ERROOO!\n");
        exit(1);
    }    

    sprintf(buffer, "REG %s %s %s %s", net, app->self.id, ip, port);
    n = sendto(fd, buffer,strlen(buffer)+1, 0, res->ai_addr, res->ai_addrlen);
    if(n == -1){
        printf("ERROOO2!\n");
        exit(1);
    } 

    addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr*) &addr, &addrlen);
    if(n == -1)
    {
        printf("ERROOO3!\n");
        exit(1);
    } 
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

    fd_set currenttSockets, testfds;
    struct timeval timeout;

    int i,out_fds,n,errcode;

    FD_ZERO(&currenttSockets); // Clear currenttSockets
    FD_SET(0,&currenttSockets); // Set standard input channel on
    printf("Size of fd_set: %ld\n",sizeof(fd_set));
    printf("Value of FD_SETSIZE: %d\n",FD_SETSIZE);
    while(1)
    {
        testfds=currenttSockets;
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
