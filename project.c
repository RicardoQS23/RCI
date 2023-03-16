// #include "project.h"
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

#define MAX_BUFFER_SIZE 248

typedef struct NODE
{
    int fd;
    char ip[16];
    char port[6];
    char id[3];
} NODE;

typedef struct INTR
{
    NODE intr[98];
    int numIntr;
} INTR;

typedef struct AppNode
{
    NODE self;
    NODE bck;
    NODE ext;
    INTR interns;
} AppNode;

enum commands
{
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
int validateCommandLine(char **cmdLine);
int validate_number(char *str);
int validate_ip(char *ip);
int validateUserInput(enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, AppNode *app);
void init(AppNode *app, char *regIP, char *regUDP, char **argv);
void commandMultiplexer(AppNode *app, char *buffer, enum commands cmd, fd_set *currentSockets, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP);
void regNetwork(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net);
void unregNetwork(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net);
void joinCommand(AppNode *app, fd_set *currentSockets, char *buffer, char *regIP, char *regUDP, char *net);
void djoinCommand(AppNode *app, fd_set *currentSockets, char *buffer, char *bootID, char *bootIP, char *bootTCP);
void leaveCommand(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net);
void showTopologyCommand(AppNode *app);
void acceptNeighbourConnection(AppNode *app, char *buffer, fd_set *currentSockets);
void closedExtConnection(AppNode *app, char *buffer, fd_set *currentSockets);
void closedIntConnection(AppNode *app, char *buffer, fd_set *currentSockets, int i);
void promoteInternToExtern(AppNode *app, char *buffer);
void writeMessageToInterns(AppNode *app, char *buffer);
void externCommunication(AppNode *app, char *buffer);
void acceptFirstConnection(AppNode *app, char *buffer, fd_set *currentSockets);
void acceptFollowingConnections(AppNode *app, char *buffer, fd_set *currentSockets);
void acceptNeighbourConnection(AppNode *app, char *buffer, fd_set *currentSockets);
void connectToBackup(AppNode *app, char *buffer, fd_set *currentSockets);
void handleInterruptions(AppNode *app, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, int counter);



int main(int argc, char *argv[])
{
    //./cot 127.0.0.1 59009 193.136.138.142 59000
    //./cot 172.26.180.206 59002 193.136.138.142 59000
    // djoin 008 11 08 127.0.0.1 59008
    // djoin 008 20 11 127.0.0.1 59011

    // IP = 127.0.0.1
    // TCP = 59001
    // regIP = 193.136.138.142
    // regUDP = 59000
    AppNode app;
    char regIP[16], regUDP[6];
    char buffer[MAX_BUFFER_SIZE] = "\0", net[4], name[16], dest[16], bootIP[16], bootID[3], bootTCP[6];
    fd_set readSockets, currentSockets;
    int counter = 0;
    struct timeval timeout;
    enum commands cmd;

    init(&app, regIP, regUDP, argv);
    FD_ZERO(&currentSockets);
    FD_SET(0, &currentSockets);

    while (1)
    {
        readSockets = currentSockets;
        memset((void *)&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 50;

        counter = select(FD_SETSIZE, &readSockets, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)&timeout);
        switch (counter)
        {
        case 0:
            printf("running ...\n");
            break;

        case -1:
            perror("select");
            exit(1);

        default:
            handleInterruptions(&app, &readSockets, &currentSockets, &cmd, buffer, bootIP, name, dest, bootID, bootTCP, net, regIP, regUDP, counter);
            break;
        }
    }
}

void closedIntConnection(AppNode *app, char *buffer, fd_set *currentSockets, int i)
{
    FD_CLR(app->interns.intr[i].fd, currentSockets);
    close(app->interns.intr[i].fd);
    memmove(&app->interns.intr[i], &app->interns.intr[app->interns.numIntr - 1], sizeof(NODE));
    app->interns.numIntr--;
}

void handleInterruptions(AppNode *app, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, int counter)
{
    for (; counter;)
    {
        if (FD_ISSET(0, readSockets))
        {
            FD_CLR(0, readSockets);
            fgets(buffer, MAX_BUFFER_SIZE, stdin);
            if (validateUserInput(cmd, buffer, bootIP, name, dest, bootID, bootTCP, net, app) < 0)
            {
                printf("bad user input\n");
                counter--;
                continue;
            }
            commandMultiplexer(app, buffer, *cmd, currentSockets, bootIP, name, dest, bootID, bootTCP, net, regIP, regUDP);
            counter--;
        }
        else if (FD_ISSET(app->self.fd, readSockets))
        {
            FD_CLR(app->self.fd, readSockets);
            printf("Someone is trying to connect!\n");
            acceptNeighbourConnection(app, buffer, currentSockets);
            counter--;
        }
        else if (FD_ISSET(app->ext.fd, readSockets)) // ext talks with us
        {
            FD_CLR(app->ext.fd, readSockets);
            if (readTcp(app->ext.fd, app, buffer) < 0) // conexao fechou e temos de estabelecer nova
            {
                printf("Extern's connection was closed\n");
                closedExtConnection(app, buffer, currentSockets);
            }
            else
            {
                printf("Extern is talking with us...\n");
                printf("recv: %s\n", buffer);
                externCommunication(app, buffer);
            }
            counter--;
        }
        else
        {
            for (int i = app->interns.numIntr - 1; i >= 0; i--)
            {
                if (FD_ISSET(app->interns.intr[i].fd, readSockets))
                {
                    FD_CLR(app->interns.intr[i].fd, readSockets);
                    if (readTcp(app->interns.intr[i].fd, app, buffer) < 0)
                    {
                        closedIntConnection(app, buffer, currentSockets, i);
                    }
                    else
                    {
                    }
                    counter--;
                }
            }
        }
    }
}

void promoteInternToExtern(AppNode *app, char *buffer)
{
    memmove(&app->ext, &app->interns.intr[0], sizeof(NODE));
    memmove(&app->interns.intr[0], &app->interns.intr[app->interns.numIntr - 1], sizeof(NODE));
    memset(&app->interns.intr[app->interns.numIntr - 1], 0, sizeof(NODE));
    app->interns.numIntr--;

    memset(buffer, 0, MAX_BUFFER_SIZE);
    sprintf(buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
    if (writeTcp(app->ext.fd, app, buffer) < 0)
    {
        printf("Can't write when I'm trying to connect\n");
        exit(1);
    }
    writeMessageToInterns(app, buffer);
}

void writeMessageToInterns(AppNode *app, char *buffer)
{
    for (int i = 0; i < app->interns.numIntr; i++)
    {
        if (writeTcp(app->interns.intr[i].fd, app, buffer) < 0)
        {
            printf("Can't write when I'm trying to connect\n");
            exit(1);
        }
    }
}

void connectToBackup(AppNode *app, char *buffer, fd_set *currentSockets)
{
    // altera o novo extern para o backup
    strcpy(app->ext.id, app->bck.id);
    strcpy(app->ext.ip, app->bck.ip);
    strcpy(app->ext.port, app->bck.port);

    if ((app->ext.fd = connectTcpClient(app)) > 0) // join network by connecting to another node
    {
        FD_SET(app->ext.fd, currentSockets); // sucessfully connected to ext node
        memset(buffer, 0, MAX_BUFFER_SIZE);
        sprintf(buffer, "NEW %s %s %s\n", app->self.id, app->self.ip, app->self.port);
        printf("sent: %s\n", buffer);
        if (writeTcp(app->ext.fd, app, buffer) < 0)
        {
            printf("Can't write when I'm trying to connect\n");
            exit(1);
        }
        if (app->interns.numIntr > 0) // tem nos intr e comunica com todos para atualizar os seus nos de backup
        {
            memset(buffer, 0, MAX_BUFFER_SIZE);
            sprintf(buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
            writeMessageToInterns(app, buffer);
        }
    }
    else
    {
        printf("TCP ERROR!!\n");
        memmove(&app->ext, &app->self, sizeof(NODE)); // couldnt connect to ext node
        exit(1);
    }
}

void closedExtConnection(AppNode *app, char *buffer, fd_set *currentSockets)
{
    if (strcmp(app->self.id, app->bck.id) == 0) // sou ancora
    {
        if (app->interns.numIntr > 0) // escolhe um dos internos para ser o seu novo externo
        {
            // promove interno a externo e retira-o da lista de internos
            FD_CLR(app->ext.fd, currentSockets);
            promoteInternToExtern(app, buffer);
        }
        else // ficou sozinho na rede
        {
            FD_CLR(app->ext.fd, currentSockets);
            close(app->ext.fd);
            memmove(&app->ext, &app->self, sizeof(NODE));
        }
    }
    else // nao é ancora
    {
        connectToBackup(app, buffer, currentSockets);
    }
}

void acceptFirstConnection(AppNode *app, char *buffer, fd_set *currentSockets)
{
    if ((app->ext.fd = acceptTcpServer(app)) > 0)
    {
        if (readTcp(app->ext.fd, app, buffer) < 0)
        {
            printf("Can't read when someone is trying to connect\n");
            exit(1);
        }
        printf("recv: %s\n", buffer);
        if (sscanf(buffer, "NEW %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port) != 3)
        {
            printf("wrong formated answer!\n");
            exit(1);
        }
        memset(buffer, 0, MAX_BUFFER_SIZE);
        sprintf(buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
        printf("sent: %s\n", buffer);
        if (writeTcp(app->ext.fd, app, buffer) < 0)
        {
            printf("Can't write when someone is trying to connect\n");
            exit(1);
        }
        FD_SET(app->ext.fd, currentSockets);
    }
    else
    {
        printf("Couldn't accept TCP connection\n");
        exit(1);
    }
}

void acceptFollowingConnections(AppNode *app, char *buffer, fd_set *currentSockets)
{
    if ((app->interns.intr[app->interns.numIntr].fd = acceptTcpServer(app)) > 0)
    {
        if (readTcp(app->interns.intr[app->interns.numIntr].fd, app, buffer) < 0)
        {
            printf("Can't read when someone is trying to connect\n");
            exit(1);
        }
        // TODO tratar da resposta no buffer
        printf("recv: %s\n", buffer);
        if (sscanf(buffer, "NEW %s %s %s\n", app->interns.intr[app->interns.numIntr].id, app->interns.intr[app->interns.numIntr].ip, app->interns.intr[app->interns.numIntr].port) != 3)
        {
            printf("wrong format\n");
            exit(1);
        }
        memset(buffer, 0, MAX_BUFFER_SIZE);
        sprintf(buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
        printf("sent: %s\n", buffer);
        if (writeTcp(app->interns.intr[app->interns.numIntr].fd, app, buffer) < 0)
        {
            printf("Can't write when someone is trying to connect\n");
            exit(1);
        }

        FD_SET(app->interns.intr[app->interns.numIntr].fd, currentSockets);
        app->interns.numIntr++;
    }
    else
    {
        printf("Couldn't accept TCP connection\n");
        exit(1);
    }
}

void acceptNeighbourConnection(AppNode *app, char *buffer, fd_set *currentSockets)
{
    // TODO antes de atribuir titulo de externo, assumir q é interno e mete-lo numa queue
    //      assim que ele receber msg do outro lado, tira-lo da queue e ascende-lo a externo
    if (strcmp(app->self.id, app->ext.id) == 0) // still alone in network
        acceptFirstConnection(app, buffer, currentSockets);
    else
        acceptFollowingConnections(app, buffer, currentSockets);
}

void externCommunication(AppNode *app, char *buffer)
{
    if (sscanf(buffer, "EXTERN %s %s %s\n", app->bck.id, app->bck.ip, app->bck.port) != 3)
    {
        printf("Bad response from server\n");
    }
    if (strcmp(app->bck.id, app->self.id) == 0)
        memmove(&app->bck, &app->self, sizeof(NODE));
}

void leaveCommand(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net)
{
    unregNetwork(app, buffer, currentSockets, regIP, regUDP, net);
    // Closing sockets
    if (strcmp(app->ext.id, app->self.id) != 0) // has extern
    {
        FD_CLR(app->ext.fd, currentSockets);
        close(app->ext.fd);
    }
    if (app->interns.numIntr > 0)
    {
        for (int i = 0; i < app->interns.numIntr; i++)
        {
            FD_CLR(app->interns.intr[i].fd, currentSockets);
            close(app->interns.intr[i].fd);
        }
        app->interns.numIntr = 0;
    }
    memmove(&app->ext, &app->self, sizeof(NODE));
    memmove(&app->ext, &app->self, sizeof(NODE));
}

void unregNetwork(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net)
{
    memset(buffer, 0, MAX_BUFFER_SIZE);
    sprintf(buffer, "UNREG %s %s", net, app->self.id);
    printf("sent: %s\n", buffer);
    udpClient(buffer, regIP, regUDP, net);
}

void regNetwork(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net)
{
    memset(buffer, 0, MAX_BUFFER_SIZE);
    sprintf(buffer, "REG %s %s %s %s\n", net, app->self.id, app->self.ip, app->self.port);
    printf("sent: %s\n", buffer);
    udpClient(buffer, regIP, regUDP, net);
    FD_SET(app->self.fd, currentSockets);
}

void djoinCommand(AppNode *app, fd_set *currentSockets, char *buffer, char *bootID, char *bootIP, char *bootTCP)
{
    memmove(&app->ext, &app->self, sizeof(NODE));
    memmove(&app->bck, &app->self, sizeof(NODE));
    strcpy(app->ext.id, bootID);
    strcpy(app->ext.ip, bootIP);
    strcpy(app->ext.port, bootTCP);

    if ((app->ext.fd = connectTcpClient(app)) > 0) // join network by connecting to another node
    {
        memset(buffer, 0, MAX_BUFFER_SIZE);
        sprintf(buffer, "NEW %s %s %s\n", app->self.id, app->self.ip, app->self.port);
        printf("sent: %s\n", buffer);
        if (writeTcp(app->ext.fd, app, buffer) < 0)
        {
            printf("Can't write when I'm trying to connect\n");
            exit(1);
        }
        FD_SET(app->ext.fd, currentSockets);
        /*memset(buffer, 0, MAX_BUFFER_SIZE);
        sprintf(buffer, "REG %s %s %s %s\n", net, app->self.id, app->self.ip, app->self.port);
        printf("sent: %s\n", buffer);
        udpClient(buffer, regIP, regUDP, net);
        FD_SET(app->self.fd, currentSockets);
        */
        // sucessfully connected to ext node
    }
    else
    {
        printf("TCP ERROR!!\n");
        memmove(&app->ext, &app->self, sizeof(NODE)); // couldnt connect to ext node
    }
}

void joinCommand(AppNode *app, fd_set *currentSockets, char *buffer, char *regIP, char *regUDP, char *net)
{
    memmove(&app->ext, &app->self, sizeof(NODE));
    memmove(&app->bck, &app->self, sizeof(NODE));
    memset(buffer, 0, MAX_BUFFER_SIZE);
    sprintf(buffer, "NODES %s", net);
    udpClient(buffer, regIP, regUDP, net); // gets net nodes
    if (sscanf(buffer, "%*s %*s %s %s %s", app->ext.id, app->ext.ip, app->ext.port) == 3)
    {
        // memmove(&app.bck, &app.ext, sizeof(NODE));
        if ((app->ext.fd = connectTcpClient(app)) > 0) // join network by connecting to another node
        {
            memset(buffer, 0, MAX_BUFFER_SIZE);
            sprintf(buffer, "NEW %s %s %s\n", app->self.id, app->self.ip, app->self.port);
            printf("sent: %s\n", buffer);
            if (writeTcp(app->ext.fd, app, buffer) < 0)
            {
                printf("Can't write when I'm trying to connect\n");
                exit(1);
            }
            // adicionar externo aos vizinhos
            regNetwork(app, buffer, currentSockets, regIP, regUDP, net);
            FD_SET(app->ext.fd, currentSockets);
        }
        else
        {
            printf("TCP ERROR!!\n");
            memmove(&app->ext, &app->self, sizeof(NODE)); // couldnt connect to ext node
        }
    }
    else
    {
        regNetwork(app, buffer, currentSockets, regIP, regUDP, net);
    }
}
void showTopologyCommand(AppNode *app)
{
    printf("---- Self info ----\n");
    printf("%s %s %s\n", app->self.id, app->self.ip, app->self.port);
    printf("---- Extern info ----\n");
    printf("%s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
    printf("---- Backup info ----\n");
    printf("%s %s %s\n", app->bck.id, app->bck.ip, app->bck.port);
    printf("---- Interns info ----\n");
    for (int i = 0; i < app->interns.numIntr; i++)
        printf("%s %s %s\n", app->interns.intr[i].id, app->interns.intr[i].ip, app->interns.intr[i].port);
}

void commandMultiplexer(AppNode *app, char *buffer, enum commands cmd, fd_set *currentSockets, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP)
{
    switch (cmd)
    {
    case JOIN:
        // Nodes's initialization
        joinCommand(app, currentSockets, buffer, regIP, regUDP, net);
        break;
    case DJOIN:
        // Nodes's initialization
        djoinCommand(app, currentSockets, buffer, bootID, bootIP, bootTCP);
        break;
    case SHOW:
        showTopologyCommand(app);
        break;
    case LEAVE:
        leaveCommand(app, buffer, currentSockets, regIP, regUDP, net);
        break;
    case EXIT:
        FD_CLR(app->self.fd, currentSockets);
        close(app->self.fd);
        exit(0);
    default:
        printf("unknown command\n");
        break;
    }
}

void init(AppNode *app, char *regIP, char *regUDP, char **argv)
{
    if (validateCommandLine(argv) < 0)
    {
        printf("wrong inputs ./cot [IP] [TCP] [regIP] [regUDP]\n");
        exit(1);
    }
    memset(app, 0, sizeof(struct AppNode));
    strcpy(app->self.ip, argv[1]);
    strcpy(app->self.port, argv[2]);
    if ((app->self.fd = openTcpServer(app)) < 0)
    {
        printf("Couldn't open TCP Server\n");
        exit(1);
    }
    strcpy(regIP, argv[3]);
    strcpy(regUDP, argv[4]);
}

int connectTcpClient(AppNode *app)
{
    int fd;
    struct addrinfo hints, *res;
    char buffer[MAX_BUFFER_SIZE];

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(app->ext.ip, app->ext.port, &hints, &res) != 0)
    {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0)
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
    if (fd == -1)
        return -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, app->self.port, &hints, &res) != 0)
    {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    if (bind(fd, res->ai_addr, res->ai_addrlen) < 0)
    {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    if (listen(fd, 5) < 0)
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
    while ((n = read(fd, buffer, MAX_BUFFER_SIZE)) < 0)
    {
        strcat(buffer, buffer_cpy);
    }

    if (n == 0 && strlen(buffer) == 0 || n == -1)
    {
        close(fd);
        return -1;
    }
    return 0;
}

int writeTcp(int fd, AppNode *app, char *buffer)
{
    if (write(fd, buffer, strlen(buffer) + 1) < 0)
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
    if ((fd = accept(app->self.fd, (struct sockaddr *)&addr, &addrlen)) == -1)
        return -1;

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
    if (fd == -1)
        exit(1);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    errcode = getaddrinfo(ip, port, &hints, &res);
    if (errcode != 0)
        exit(1);

    n = sendto(fd, buffer, strlen(buffer) + 1, 0, res->ai_addr, res->ai_addrlen);
    if (n == -1)
        exit(1);

    addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&addr, &addrlen);
    if (n == -1)
        exit(1);

    write(1, buffer, n);
    write(1, "\n", 2);
    freeaddrinfo(res);
    close(fd);
}

int compare_cmd(char *cmdLine)
{
    char cmds[8][7] = {
        "join",
        "djoin",
        "create",
        "delete",
        "get",
        "leave",
        "exit",
        "show",
    };
    for (int i = 0; i < 8; i++)
    {
        if (strcmp(cmdLine, cmds[i]) == 0)
            return i;
    }
    return -1;
}

int validateCommandLine(char **cmdLine)
{
    // Check machineIP
    if (validate_ip(cmdLine[1]) < 0)
        return -1;
    // Check TCP
    if (strlen(cmdLine[2]) > 5)
        return -1;
    // Check regIP
    if (validate_ip(cmdLine[3]) < 0)
        return -1;
    // Check regUDP
    if (strlen(cmdLine[4]) > 5)
        return -1;
    return 0;
}

int validate_number(char *str)
{
    while (*str != '\0')
    {
        if (!isdigit(*str))
            return 0; // if the character is not a number, return false
        str++;        // point to next character
    }
    return 1;
}

int validate_ip(char *ip)
{ // check whether the IP is valid or not
    int i, num, dots = 0;
    char *ptr, copy[64];
    strcpy(copy, ip);

    if (copy == NULL)
        return -1;
    ptr = strtok(copy, "."); // cut the string using dor delimiter
    if (ptr == NULL)
        return -1;

    while (ptr)
    {
        if (!validate_number(ptr))
            return -1;   // check whether the sub string is  holding only number or not
        num = atoi(ptr); // convert substring to number
        if (num >= 0 && num <= 255)
        {
            ptr = strtok(NULL, "."); // cut the next part of the string
            if (ptr != NULL)
                dots++; // increase the dot count
        }
        else
            return -1;
    }
    if (dots != 3)
        return -1; // if the number of dots are not 3, return false

    return 0;
}

int validateUserInput(enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, AppNode *app)
{
    char *token;
    int cmd_code;
    token = strtok(buffer, " \n\r\t");
    cmd_code = compare_cmd(token);
    switch (cmd_code)
    {
    case 0: // Join
        *cmd = JOIN;

        if ((token = strtok(NULL, " \n\r\t")) == NULL)
            return -1;

        if (strlen(token) != 3 && ((atoi(token) > 100) || atoi(token) < 0))
            return -1;
        strcpy(net, token);

        if ((token = strtok(NULL, " \n\r\t")) == NULL)
            return -1;

        if (strlen(token) != 2 && ((atoi(token) > 99) || atoi(token) < 0))
            return -1;
        strcpy(app->self.id, token);
        break;
    case 1: // Djoin
        *cmd = DJOIN;
        if ((token = strtok(NULL, " \n\r\t")) == NULL)
            return -1;
        if (strlen(token) != 3 && ((atoi(token) > 100) || atoi(token) < 0))
            return -1;
        strcpy(net, token);

        if ((token = strtok(NULL, " \n\r\t")) == NULL)
            return -1;
        if (strlen(token) != 2 && ((atoi(token) > 99) || atoi(token) < 0))
            return -1;
        strcpy(app->self.id, token);

        if ((token = strtok(NULL, " \n\r\t")) == NULL)
            return -1;
        if (strlen(token) != 2 && ((atoi(token) > 100) || atoi(token) < 0))
            return -1;
        strcpy(bootID, token);

        if ((token = strtok(NULL, " \n\r\t")) == NULL)
            return -1;
        strcpy(bootIP, token);

        if ((token = strtok(NULL, " \n\t\r")) == NULL)
            return -1;
        strcpy(bootTCP, token);
        if (strlen(bootTCP) != 5)
            return -1;

        if (validate_ip(bootIP) != 0)
            return -1;
        break;
    case 2:
        *cmd = CREATE;
        if ((token = strtok(NULL, " \n\r\t")) == NULL)
            return -1;
        strcpy(name, token); // Adcionar campo name à struct???
        break;
    case 3:
        *cmd = DELETE;
        if ((token = strtok(NULL, " \n\r\t")) == NULL)
            return -1;
        strcpy(name, token); // Adcionar campo name à struct???
        break;
    case 4:
        *cmd = GET;
        if ((token = strtok(NULL, " \n\r\t")) == NULL)
            return -1;

        strcpy(dest, token);
        if ((token = strtok(NULL, " \n\r\t")) == NULL)
            return -1;

        strcpy(name, token);
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
        *cmd = -1;
        printf("Unknown Command\n");
        break;
    }
    return 0;
}
