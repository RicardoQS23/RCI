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

#define MAX_BUFFER_SIZE 512

typedef struct NODE{ 
    int fd;
    char ip[16];
    char port[6];
    char id[3];
}NODE;

typedef struct NEIGH{
    NODE intr[98];
    int numIntr;
}NEIGH;

typedef struct AppNode{
    NODE self;
    NODE bck;
    NODE ext;
    NEIGH neighbours; 
}AppNode;

enum commands{
    JOIN,
    DJOIN,
    CREATE,
    SHOW,
    DELETE,
    GET,
    LEAVE,
    EXIT
};

int compare_cmd(char *cmdLine);
void udpClient(char *buffer, char *ip, char *port, char *net);
int openTcpServer(AppNode *app);
int acceptTcpServer(AppNode *app);
int readTcp(int fd, AppNode *app, char *buffer);
int writeTcp(int fd, AppNode *app, char *buffer);
int connectTcpClient(AppNode *app);
int handleCommandLine(char **cmdLine);
int validate_number(char *str);
int validate_ip(char *ip);
int handleUserInput(enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootid, char *bootTCP, char *net, AppNode *app);

int main(int argc, char *argv[])
{
    //TODO perceber como é q os diferentes nós se voltam a ligar aos backups
    //TODO tratar do userInput do DJOIN
    //./cot 127.0.0.1 59005 193.136.138.142 59000
    //djoin 008 05 02 127.0.0.1 59002

    //IP = 127.0.0.1
    //TCP = 59001
    //regIP = 193.136.138.142
    //regUDP = 59000
    AppNode app;
    char regIP[16], regUDP[6];
    char buffer[MAX_BUFFER_SIZE], net[4], name[16], dest[16], bootIP[16], bootid[3], bootTCP[6];
    fd_set readSockets, currentSockets;
    int newfd, counter;
    struct timeval timeout;
    enum commands cmd;

    if(handleCommandLine(argv) < 0)
    {
        printf("wrong inputs ./cot [IP] [TCP] [regIP] [regUDP]\n");
        exit(1);
    }
    app.neighbours.numIntr = 0;
    strcpy(app.self.ip, argv[1]);
    strcpy(app.self.port, argv[2]);
    strcpy(regIP, argv[3]);
    strcpy(regUDP, argv[4]);

    FD_ZERO(&currentSockets);    
    FD_SET(0,&currentSockets);  

    while(1)
    {
        readSockets = currentSockets;
        memset((void *)&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 20;

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
                for(;counter;)
                {
                    if(FD_ISSET(0, &readSockets))
                    {   
                        fgets(buffer, MAX_BUFFER_SIZE, stdin);
                        printf("buffer: %s\n", buffer);
                        if(handleUserInput(&cmd, buffer, bootIP, name, dest, bootid, bootTCP, net, &app) < 0)
                        {
                            printf("bad input\n");
                            exit(1);
                        }
                        switch(cmd){
                            case JOIN:
                                //Nodes's initialization
                                memmove(&app.ext, &app.self, sizeof(NODE));
                                memmove(&app.bck, &app.self, sizeof(NODE));
                                sprintf(buffer, "NODES %s", net);
                                udpClient(buffer, regIP, regUDP, net);  //gets net nodes
                                if(sscanf(buffer, "%*s %*s %s %s %s", app.ext.id, app.ext.ip, app.ext.port) == 3)
                                {
                                    //memmove(&app.bck, &app.ext, sizeof(NODE));
                                    if((app.ext.fd = connectTcpClient(&app)) > 0)   //join network by connecting to another node
                                    {
                                        FD_SET(app.ext.fd, &currentSockets);  //sucessfully connected to ext node
                                        sprintf(buffer, "NEW %s %s %s\n", app.self.id, app.self.ip, app.self.port);
                                        printf("sent: %s\n", buffer);
                                        if(writeTcp(app.ext.fd, &app, buffer) < 0)
                                        {
                                            printf("Can't write when I'm trying to connect\n");
                                            exit(1);
                                        }
                                        if(readTcp(app.ext.fd, &app, buffer) < 0)
                                        {
                                            printf("Can't read when I'm trying to connect\n");
                                            exit(1);
                                        }
                                        if(sscanf(buffer, "EXTERN %s %s %s\n", app.bck.id, app.bck.ip, app.bck.port) != 3)
                                        {
                                            printf("Bad response from server\n");
                                        }
                                        printf("recv: %s\n", buffer);
                                        if(strcmp(app.bck.id, app.self.id) == 0)    memmove(&app.bck, &app.self, sizeof(NODE)); //There's only one node on the network, therefore I'm my own bck

                                        //adicionar externo aos vizinhos
                                        if((app.self.fd = openTcpServer(&app)) > 0)
                                        {
                                            sprintf(buffer, "REG %s %s %s %s\n", net, app.self.id, app.self.ip, app.self.port);
                                            printf("sent: %s\n", buffer);
                                            udpClient(buffer, regIP, regUDP, net);
                                            FD_SET(app.self.fd, &currentSockets);
                                        }
                                        else
                                        {
                                            printf("Couldn't join network\n");
                                        }  
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
                                    if((app.self.fd = openTcpServer(&app)) > 0)
                                    {
                                        /*printf("net: %s\n", net);
                                        printf("id: %s\n", app.self.id);
                                        printf("ip: %s\n", app.self.ip);
                                        printf("port: %s\n", app.self.port);*/

                                        sprintf(buffer, "REG %s %s %s %s\n", net, app.self.id, app.self.ip, app.self.port);
                                        printf("sent: %s\n", buffer);
                                        udpClient(buffer, regIP, regUDP, net);
                                        FD_SET(app.self.fd, &currentSockets);
                                    }
                                    else
                                    {
                                        printf("Couldn't join network\n");
                                    }
                                }
                                break;
                            case DJOIN:
                                //Nodes's initialization
                                memmove(&app.ext, &app.self, sizeof(NODE));
                                memmove(&app.bck, &app.self, sizeof(NODE));

                                strcpy(app.ext.id, bootid);
                                strcpy(app.ext.ip, bootIP);
                                strcpy(app.ext.port, bootTCP);

                                //memmove(&app.bck, &app.ext, sizeof(NODE));
                                if((app.ext.fd = connectTcpClient(&app)) > 0)   //join network by connecting to another node
                                {
                                    FD_SET(app.ext.fd, &currentSockets);  //sucessfully connected to ext node
                                    sprintf(buffer, "NEW %s %s %s\n", app.self.id, app.self.ip, app.self.port);
                                    printf("sent: %s\n", buffer);
                                    if(writeTcp(app.ext.fd, &app, buffer) < 0)
                                    {
                                        printf("Can't write when I'm trying to connect\n");
                                        exit(1);
                                    }
                                    if(readTcp(app.ext.fd, &app, buffer) < 0)
                                    {
                                        printf("Can't read when I'm trying to connect\n");
                                        exit(1);
                                    }
                                    if(sscanf(buffer, "EXTERN %s %s %s\n", app.bck.id, app.bck.ip, app.bck.port) != 3)
                                    {
                                        printf("Bad response from server\n");
                                    }
                                    printf("recv: %s\n", buffer);
                                    if(strcmp(app.bck.id, app.self.id) == 0)    memmove(&app.bck, &app.self, sizeof(NODE)); //There's only one node on the network, therefore I'm my own bck

                                    if((app.self.fd = openTcpServer(&app)) > 0)
                                    {
                                        sprintf(buffer, "REG %s %s %s %s\n", net, app.self.id, app.self.ip, app.self.port);
                                        printf("sent: %s\n", buffer);
                                        udpClient(buffer, regIP, regUDP, net);
                                        FD_SET(app.self.fd, &currentSockets);
                                    }
                                    else
                                    {
                                        printf("Couldn't join network\n");
                                    }  
                                }
                                else
                                {
                                    printf("TCP ERROR!!\n");
                                    memmove(&app.ext, &app.self, sizeof(NODE)); //couldnt connect to ext node
                                } 
                                break;
                            case SHOW:
                                printf("My info: \n");
                                printf("%s %s %s\n", app.self.id, app.self.ip, app.self.port);
                                printf("Ext info: \n");
                                printf("%s %s %s\n", app.ext.id, app.ext.ip, app.ext.port);
                                printf("Bck info: \n");
                                printf("%s %s %s\n", app.bck.id, app.bck.ip, app.bck.port);
                                printf("Neighbours info: \n");
                                for(int i = 0; i < app.neighbours.numIntr; i++)
                                    printf("%s %s %s\n", app.neighbours.intr[i].id, app.neighbours.intr[i].ip, app.neighbours.intr[i].port);

                                break;
                            case LEAVE:
                                //Closing sockets sockets
                                close(app.self.fd);
                                close(app.ext.fd);
                                for(int i = 0; i < app.neighbours.numIntr; i++)
                                    close(app.neighbours.intr[app.neighbours.numIntr].fd);   
                                
                                sprintf(buffer, "UNREG %s %s", net, app.self.id);
                                printf("sent: %s\n", buffer);
                                udpClient(buffer, regIP, regUDP, net);
                                break;
                            default:
                                printf("default\n");
                                break;
                        }
                        counter--;
                    }
                    else if(FD_ISSET(app.self.fd, &readSockets))
                    {
                        printf("Someone is trying to connect!\n");
                        printf("self id: %s ext id: %s\n", app.self.id, app.ext.id);
                        if(strcmp(app.self.id,app.ext.id) == 0) //still alone in network
                        {
                            if((app.ext.fd = acceptTcpServer(&app)) > 0)
                            {
                                if(readTcp(app.ext.fd, &app, buffer) < 0)
                                {
                                    printf("Can't read when someone is trying to connect\n");
                                }
                                printf("recv: %s\n", buffer);
                                if(sscanf(buffer, "NEW %s %s %s\n", app.ext.id, app.ext.ip, app.ext.port) != 3)
                                {
                                    printf("wrong formated answer!\n");
                                    exit(1);
                                }
                                printf("ext id: %s\n",app.ext.id);
                                printf("ext ip: %s\n",app.ext.ip);
                                printf("ext port: %s\n",app.ext.port);

                                //TODO tratar da resposta no buffer

                                sprintf(buffer, "EXTERN %s %s %s\n", app.ext.id, app.ext.ip, app.ext.port);
                                printf("sent: %s\n", buffer);
                                if(writeTcp(app.ext.fd, &app, buffer) < 0)
                                {
                                    printf("Can't write when someone is trying to connect\n");
                                }
                                FD_SET(app.ext.fd, &currentSockets);
                            }
                            else
                            {
                                printf("Couldn't accept TCP connection\n");
                            }
                        }
                        else
                        {
                            if((app.neighbours.intr[app.neighbours.numIntr].fd = acceptTcpServer(&app)) > 0)
                            {
                                if(readTcp(app.neighbours.intr[app.neighbours.numIntr].fd, &app, buffer) < 0)
                                {
                                    printf("Can't read when someone is trying to connect\n");
                                }
                                //TODO tratar da resposta no buffer
                                printf("recv: %s\n", buffer);
                                if(sscanf(buffer, "NEW %s %s %s\n", app.neighbours.intr[app.neighbours.numIntr].id, app.neighbours.intr[app.neighbours.numIntr].ip, app.neighbours.intr[app.neighbours.numIntr].port) != 3)
                                {
                                    printf("wrong format\n");
                                    exit(1);
                                }
                                sprintf(buffer, "EXTERN %s %s %s\n", app.ext.id, app.ext.ip, app.ext.port);
                                printf("sent: %s\n", buffer);
                                if(writeTcp(app.neighbours.intr[app.neighbours.numIntr].fd, &app, buffer) < 0)
                                {
                                    printf("Can't write when someone is trying to connect\n");
                                }

                                FD_SET(app.neighbours.intr[app.neighbours.numIntr].fd, &currentSockets);
                                app.neighbours.numIntr++;
                            }
                            else
                            {
                                printf("Couldn't accept TCP connection\n");
                            }
                        }
                        counter--;

                    }
                    else if(FD_ISSET(app.ext.fd, &readSockets))
                    {
                        //ext talks with us
                        printf("ext fd\n");
                        counter--;
                    }
                    else 
                    {
                        for(int i = 0; i < app.neighbours.numIntr; i++)
                        {
                            if(FD_ISSET(app.neighbours.intr[i].fd, &readSockets))
                            {
                                if(readTcp(app.neighbours.intr[i].fd, &app, buffer) < 0)
                                {
                                    printf("error when writing\n");
                                    exit(1);
                                }
                                counter--;
                            }
                        }
                    }
                }
        }
    }
    return 0;
}


int connectTcpClient(AppNode *app)
{
    int fd;
    struct addrinfo hints, *res;
    char buffer[MAX_BUFFER_SIZE];

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1)    return -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(app->ext.ip, app->ext.port, &hints, &res) != 0)
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

    return fd;
}

int openTcpServer(AppNode *app)
{
    int fd;
    struct addrinfo hints, *res;
    char buffer[MAX_BUFFER_SIZE];

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1)    return -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if(getaddrinfo(NULL, app->self.port, &hints, &res) != 0)
    {
        printf("CAGA1\n");
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    if(bind(fd, res->ai_addr, res->ai_addrlen) < 0)
    {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    if(listen(fd, 5) < 0)
    {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    return fd;
}

int readTcp(int fd, AppNode *app, char *buffer)
{
    char *buffer_cpy;
    ssize_t n;
    buffer[0] = '\0';
    while((n = read(fd, buffer, MAX_BUFFER_SIZE)) < 0)
    {
        strcat(buffer, buffer_cpy);
    } 

    if(n == 0 && strlen(buffer) == 0 || n == -1)
    {
        close(fd);        
        return -1;
    }
    return 0;
}

int writeTcp(int fd, AppNode *app, char *buffer)
{
    if(write(fd, buffer, strlen(buffer)+1) < 0)
    {
        close(fd);
        return -1;
    }
    return 0;
}

int acceptTcpServer(AppNode *app)
{
    int fd;
    struct sockaddr_in addr;
    char buffer[MAX_BUFFER_SIZE];
    socklen_t addrlen;

    addrlen = sizeof(addr);
    if((fd = accept(app->self.fd,(struct sockaddr*) &addr, &addrlen)) == -1)    return -1;
    
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

    n = sendto(fd, buffer, strlen(buffer)+1, 0, res->ai_addr, res->ai_addrlen);
    if(n == -1)     exit(1);

    addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr*) &addr, &addrlen);
    if(n == -1)    exit(1);

    write(1, buffer, n);write(1,"\n",2);
    freeaddrinfo(res);
    close(fd);
}

int compare_cmd(char *cmdLine)
{
    char cmds[8][7] = {"join", "djoin", "create", "delete", "get", "leave", "exit", "show",};
    for (int i = 0; i < 8; i++)
    {
        if(strcmp(cmdLine,cmds[i]) == 0)
            return i;
    }
    return -1;
}

int handleCommandLine(char **cmdLine)
{
    // Check machineIP
    if(validate_ip(cmdLine[1]) < 0)
        return -1;
        // Check TCP
    if(strlen(cmdLine[2]) > 5)
        return -1;
    // Check regIP
    if(validate_ip(cmdLine[3]) < 0)
        return -1;
     // Check regUDP
    if(strlen(cmdLine[4]) > 5)
        return -1;
    return 0;
}

int validate_number(char *str) {
    while (*str != '\0')
    {
      if(!isdigit(*str))    return 0; //if the character is not a number, return false
      str++; //point to next character
    }
    return 1;
}

int validate_ip(char *ip) { //check whether the IP is valid or not
    int i, num, dots = 0;
    char *ptr, copy[64];
    strcpy(copy, ip);

    if (copy == NULL)   return -1;
    ptr = strtok(copy, "."); //cut the string using dor delimiter
    if (ptr == NULL)    return -1;
            
    while (ptr) 
    {
        if (!validate_number(ptr))    return -1; //check whether the sub string is  holding only number or not          
        num = atoi(ptr); //convert substring to number
        if (num >= 0 && num <= 255)
        {
            ptr = strtok(NULL, "."); //cut the next part of the string
            if (ptr != NULL)    dots++; //increase the dot count
        }
        else
            return -1;
    }
    if (dots != 3)  return -1; //if the number of dots are not 3, return false
        
    return 0;
}

int handleUserInput(enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootid, char *bootTCP, char *net, AppNode *app)
{
    char *token;
    int cmd_code;
    token = strtok(buffer, " \n\r\t");
    cmd_code = compare_cmd(token);
    switch(cmd_code)
    {
        case 0: //Join
            *cmd = JOIN;

            if((token = strtok(NULL," ")) == NULL)  return -1;

            if(strlen(token) != 3 && ((atoi(token) > 100) || atoi(token) < 0))
                return -1;
            strcpy(net,token);

            if((token = strtok(NULL," ")) == NULL)  return -1;
            
            if(strlen(token) != 2 && ((atoi(token) > 99) || atoi(token) < 0))
                return -1;
            strcpy(app->self.id,token);
            break;
        case 1 : //Djoin
            *cmd = DJOIN;
            if((token = strtok(NULL," ")) == NULL)
            {
                printf("1*\n");
                return -1;
            }

            if(strlen(token) != 3 && ((atoi(token) > 100) || atoi(token) < 0))
            {
                printf("1\n");
                return -1;
            }
            strcpy(net, token);
            printf("%s\n", net);
            
            if((token = strtok(NULL," ")) == NULL)
            {
                printf("2*\n");
                return -1;
            }

            if(strlen(token) != 2 && ((atoi(token) > 99) || atoi(token) < 0))
            {
                printf("2\n");
                return -1;
            }
            strcpy(app->self.id, token);
            printf("%s\n", app->self.id);

            if((token = strtok(NULL," ")) == NULL)
            {
                printf("3*\n");
                return -1;
            }

            if(strlen(token) != 2 && ((atoi(token) > 100) || atoi(token) < 0))
            {
                printf("3\n");
                return -1;
            }
            strcpy(bootid, token);
            printf("%s\n", bootid);

            if((token = strtok(NULL," ")) == NULL)
            {
                printf("3*\n");
                return -1;
            }
            if(validate_ip(token) != 0)
            {
                printf("4\n");
                return -1;
            }

            strcpy(bootIP,token);
            printf("%s\n", bootIP);
            printf("token: %s\n", token);
            
            if((token = strtok(NULL," ")) == NULL)
            {
                printf("4*\n");
                return -1;
            }
            printf("token1: %s\n", token);

            strcpy(bootTCP,token);
            printf("%s\n", bootTCP);
            break;
        case 2:
            *cmd = CREATE;
            if((token = strtok(NULL," ")) == NULL)  return -1;
            strcpy(name,token);     // Adcionar campo name à struct???
            break;
        case 3:
            *cmd = DELETE;
            if((token = strtok(NULL," ")) == NULL)  return -1;
            strcpy(name,token);       // Adcionar campo name à struct???
            break;
        case 4:
            *cmd = GET;
            if((token = strtok(NULL," ")) == NULL)  return -1;

            strcpy(dest,token);
            if((token = strtok(NULL," ")) == NULL)  return -1; 

            strcpy(name,token);         
            break; 
        case 5:
            *cmd = LEAVE;
            break;
        case 6:
            *cmd = EXIT;
            exit(1);
            break;
        case 7:
            *cmd = SHOW;
            break;
        default:
            printf("Unknown Command\n");
            break;
    }   
    return 0;                            
}