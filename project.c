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

typedef struct LinkedList
{
    char contentName[101];
    struct LinkedList *next;
} LinkedList;

typedef struct NODE
{
    int fd;
    char ip[16];
    char port[6];
    char id[3];
    LinkedList *contentList;
} NODE;

typedef struct NodeQueue
{
    NODE queue[5];
    int numNodesInQueue;
} NodeQueue;

typedef struct INTR
{
    NODE intr[98];
    int numIntr;
} INTR;

typedef struct ExpeditionTable
{
    int fd;
    char id[3];
} ExpeditionTable;

typedef struct AppNode
{
    NODE self;
    NODE bck;
    NODE ext;
    INTR interns;
    ExpeditionTable expeditionTable[99];
} AppNode;

enum commands
{
    JOIN,
    DJOIN,
    CREATE,
    DELETE,
    GET,
    SHOW_NAMES,
    SHOW_TOPOLOGY,
    SHOW_ROUTING,
    LEAVE,
    EXIT
};

void commandMultiplexer(AppNode *app, char *buffer, enum commands cmd, fd_set *currentSockets, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP);
void joinCommand(AppNode *app, fd_set *currentSockets, char *buffer, char *regIP, char *regUDP, char *net);
void djoinCommand(AppNode *app, fd_set *currentSockets, char *buffer, char *bootID, char *bootIP, char *bootTCP);
void leaveCommand(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net);
void createCommand(AppNode *app, char *name);
void deleteCommand(AppNode *app, char *name);
void getCommand(AppNode *app, char *dest, char *name);
void showTopologyCommand(AppNode *app);
void showNamesCommand(AppNode *app);
void showRoutingCommand(AppNode *app);

int validateCommandLine(char **cmdLine);
int validate_number(char *str);
int validate_ip(char *ip);
int validateUserInput(enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, AppNode *app);
int compare_cmd(char *cmdLine);
char *advancePointer(char *token);
void chooseRandomNodeToConnect(char *buffer);

int searchContentOnList(AppNode *app, char *name);
void freeContentList(AppNode *app);

void updateExpeditionTable(AppNode *app, char *dest_id, char *neigh_id, int neigh_fd);

void udpClient(char *buffer, char *ip, char *port, char *net);
void regNetwork(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net);
void unregNetwork(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net);

int openTcpServer(AppNode *app);
int acceptTcpServer(AppNode *app);
void acceptNeighbourConnection(AppNode *app, NodeQueue *queue, fd_set *currentSockets);
// void acceptFirstConnection(AppNode *app, char *buffer, fd_set *currentSockets);
// void acceptFollowingConnections(AppNode *app, char *buffer, fd_set *currentSockets);
int connectTcpClient(AppNode *app);
void connectToBackup(AppNode *app, char *buffer, fd_set *currentSockets);
void closedExtConnection(AppNode *app, char *buffer, fd_set *currentSockets);
void closedIntConnection(AppNode *app, fd_set *currentSockets, int i);
void promoteInternToExtern(AppNode *app, char *buffer);
void externCommunication(AppNode *app, char *buffer);
void internCommunication(AppNode *app, NODE node, char *buffer);
void writeMessageToInterns(AppNode *app, char *buffer);
int writeTcp(int fd, char *buffer);
int readTcp(int fd, char *buffer);

void cleanQueue(NodeQueue *queue, fd_set *currentSockets);
void popQueue(NodeQueue *queue, fd_set *currentSockets, int pos);
void promoteQueueToIntern(AppNode *app, NodeQueue *queue, fd_set *currentSockets, int pos);

void init(AppNode *app, NodeQueue *queue, char *regIP, char *regUDP, char **argv);
void handleInterruptions(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP);
void handleQueueInterruptions(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets, char *buffer);
void handleInternInterruptions(AppNode *app, fd_set *readSockets, fd_set *currentSockets, char *buffer);
void handleUserInputInterruption(AppNode *app, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP);
void handleServerInterruption(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets);
void handleExtInterruption(AppNode *app, char *buffer, fd_set *readSockets, fd_set *currentSockets);

void handleEXTmessage(AppNode *app, char *cmd, char *token);
void handleQUERYmessage(AppNode *app, NODE node, char *cmd, char *token);
void shareQUERYmessages(AppNode *app, NODE node, char *buffer, char *dest_id);
void handleCONTENTmessage(AppNode *app, char *cmd, char *buffer);

int main(int argc, char *argv[])
{
    //./cot 172.26.180.206 59002 193.136.138.142 59000
    //./cot 127.0.0.1 59009 193.136.138.142 59000
    // djoin 008 20 11 127.0.0.1 59011

    AppNode app;
    NodeQueue queue;
    char regIP[16], regUDP[6];
    char buffer[MAX_BUFFER_SIZE] = "\0", net[4], name[16], dest[16], bootIP[16], bootID[3], bootTCP[6];
    fd_set readSockets, currentSockets;
    int counter = 0;
    struct timeval timeout;
    enum commands cmd;

    init(&app, &queue, regIP, regUDP, argv);
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
            if (queue.numNodesInQueue > 0)
                cleanQueue(&queue, &currentSockets);
            break;

        case -1:
            perror("select");
            exit(1);

        default:
            handleInterruptions(&app, &queue, &readSockets, &currentSockets, &cmd, buffer, bootIP, name, dest, bootID, bootTCP, net, regIP, regUDP);
            break;
        }
        counter = 0;
    }
}

char *advancePointer(char *token)
{
    return token + strlen(token) + 1;
}

void cleanQueue(NodeQueue *queue, fd_set *currentSockets)
{
    for (int i = queue->numNodesInQueue; i >= 0; i--)
    {
        FD_CLR(queue->queue[i].fd, currentSockets);
        close(queue->queue[i].fd);
    }
    memset(&queue, 0, sizeof(NodeQueue));
}

void closedIntConnection(AppNode *app, fd_set *currentSockets, int i)
{
    FD_CLR(app->interns.intr[i].fd, currentSockets);
    close(app->interns.intr[i].fd);
    memmove(&app->interns.intr[i], &app->interns.intr[app->interns.numIntr - 1], sizeof(NODE));
    memset(&app->interns.intr[app->interns.numIntr - 1], 0, sizeof(NODE));
    app->interns.numIntr--;
}
void handleUserInputInterruption(AppNode *app, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP)
{
    if (FD_ISSET(0, readSockets))
    {
        FD_CLR(0, readSockets);
        fgets(buffer, MAX_BUFFER_SIZE, stdin);
        if (validateUserInput(cmd, buffer, bootIP, name, dest, bootID, bootTCP, net, app) < 0)
        {
            printf("bad user input\n");

            return;
        }
        commandMultiplexer(app, buffer, *cmd, currentSockets, bootIP, name, dest, bootID, bootTCP, net, regIP, regUDP);
    }
}
void handleServerInterruption(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets)
{
    if (FD_ISSET(app->self.fd, readSockets))
    {
        printf("Someone is trying to connect!\n");
        FD_CLR(app->self.fd, readSockets);
        acceptNeighbourConnection(app, queue, currentSockets);
    }
}

void handleExtInterruption(AppNode *app, char *buffer, fd_set *readSockets, fd_set *currentSockets)
{
    if (FD_ISSET(app->ext.fd, readSockets)) // ext talks with us
    {
        FD_CLR(app->ext.fd, readSockets);
        if (readTcp(app->ext.fd, buffer) < 0) // conexao fechou e temos de estabelecer nova
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
    }
}

void handleInterruptions(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP)
{
    handleUserInputInterruption(app, readSockets, currentSockets, cmd, buffer, bootIP, name, dest, bootID, bootTCP, net, regIP, regUDP);
    handleServerInterruption(app, queue, readSockets, currentSockets);
    handleExtInterruption(app, buffer, readSockets, currentSockets);
    handleInternInterruptions(app, readSockets, currentSockets, buffer);
    handleQueueInterruptions(app, queue, readSockets, currentSockets, buffer);
}

void handleQueueInterruptions(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets, char *buffer)
{
    for (int i = queue->numNodesInQueue - 1; i >= 0; i--)
    {
        if (FD_ISSET(queue->queue[i].fd, readSockets))
        {
            FD_CLR(queue->queue[i].fd, readSockets);
            if (readTcp(queue->queue[i].fd, buffer) < 0)
            {
                FD_CLR(queue->queue[i].fd, currentSockets);
                close(queue->queue[i].fd);
                popQueue(queue, currentSockets, i);
            }
            else
            {
                printf("recv: %s\n", buffer);
                // validate msg read
                if (sscanf(buffer, "NEW %s %s %s\n", queue->queue[i].id, queue->queue[i].ip, queue->queue[i].port) != 3)
                {
                    printf("wrong format\n");
                    // TODO descartar nó malicioso da queue
                    exit(1);
                }

                promoteQueueToIntern(app, queue, currentSockets, i);
                if (strcmp(app->ext.id, app->self.id) == 0)
                    promoteInternToExtern(app, buffer);
                else
                {
                    memset(buffer, 0, MAX_BUFFER_SIZE);
                    sprintf(buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
                    printf("sent: %s\n", buffer);
                    if (writeTcp(app->interns.intr[app->interns.numIntr - 1].fd, buffer) < 0)
                    {
                        printf("Can't write when someone is trying to connect\n");
                        exit(1);
                    }
                }
            }
        }
    }
}

void handleInternInterruptions(AppNode *app, fd_set *readSockets, fd_set *currentSockets, char *buffer)
{
    for (int i = app->interns.numIntr - 1; i >= 0; i--)
    {
        if (FD_ISSET(app->interns.intr[i].fd, readSockets))
        {
            FD_CLR(app->interns.intr[i].fd, readSockets);
            if (readTcp(app->interns.intr[i].fd, buffer) < 0)
            {
                closedIntConnection(app, currentSockets, i);
            }
            else
            {
                internCommunication(app, app->interns.intr[i], buffer);
            }
        }
    }
}

void promoteQueueToIntern(AppNode *app, NodeQueue *queue, fd_set *currentSockets, int pos)
{
    app->interns.numIntr++;
    memmove(&app->interns.intr[app->interns.numIntr - 1], &queue->queue[pos], sizeof(NODE));
    popQueue(queue, currentSockets, pos);
    updateExpeditionTable(app, app->interns.intr[app->interns.numIntr - 1].id, app->interns.intr[app->interns.numIntr - 1].id, app->interns.intr[app->interns.numIntr - 1].fd);
}

void popQueue(NodeQueue *queue, fd_set *currentSockets, int pos)
{
    memmove(&queue->queue[pos], &queue->queue[queue->numNodesInQueue - 1], sizeof(NODE));
    memset(&queue->queue[queue->numNodesInQueue - 1], 0, sizeof(NODE));
    queue->numNodesInQueue--;
}

void promoteInternToExtern(AppNode *app, char *buffer)
{
    memmove(&app->ext, &app->interns.intr[0], sizeof(NODE));
    memmove(&app->interns.intr[0], &app->interns.intr[app->interns.numIntr - 1], sizeof(NODE));
    memset(&app->interns.intr[app->interns.numIntr - 1], 0, sizeof(NODE));
    app->interns.numIntr--;

    memset(buffer, 0, MAX_BUFFER_SIZE);
    sprintf(buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
    if (writeTcp(app->ext.fd, buffer) < 0)
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
        if (writeTcp(app->interns.intr[i].fd, buffer) < 0)
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
        if (writeTcp(app->ext.fd, buffer) < 0)
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
        updateExpeditionTable(app, app->ext.id, app->ext.id, app->ext.fd);
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
/*
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
}*/

/*
        if (readTcp(app->interns.intr[queue->numNodesInQueue].fd, app, buffer) < 0)
        {
            printf("Can't read when someone is trying to connect\n");
            exit(1);
        }
        // TODO tratar da resposta no buffer
        printf("recv: %s\n", buffer);
        if (sscanf(buffer, "NEW %s %s %s\n", app->interns.intr[queue->numNodesInQueue].id, app->interns.intr[queue->numNodesInQueue].ip, app->interns.intr[queue->numNodesInQueue].port) != 3)
        {
            printf("wrong format\n");
            exit(1);
        }
        memset(buffer, 0, MAX_BUFFER_SIZE);
        sprintf(buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
        printf("sent: %s\n", buffer);
        if (writeTcp(app->interns.intr[queue->numNodesInQueue].fd, app, buffer) < 0)
        {
            printf("Can't write when someone is trying to connect\n");
            exit(1);
        }

*/

void acceptNeighbourConnection(AppNode *app, NodeQueue *queue, fd_set *currentSockets)
{
    // TODO antes de atribuir titulo de externo, assumir q é interno e mete-lo numa queue
    //      assim que ele receber msg do outro lado, tira-lo da queue e ascende-lo a externo
    if ((queue->queue[queue->numNodesInQueue].fd = acceptTcpServer(app)) > 0)
    {
        FD_SET(queue->queue[queue->numNodesInQueue].fd, currentSockets);
        queue->numNodesInQueue++;
    }
    else
    {
        printf("Couldn't accept TCP connection\n");
        exit(1);
    }
}
void handleEXTmessage(AppNode *app, char *cmd, char *token)
{
    if (strcmp(cmd, "EXTERN") == 0)
    {
        if (sscanf(token, "EXTERN %s %s %s", app->bck.id, app->bck.ip, app->bck.port) != 3)
        {
            printf("Bad response from server\n");
        }
        if (strcmp(app->bck.id, app->self.id) == 0)
            memmove(&app->bck, &app->self, sizeof(NODE));
        else
            updateExpeditionTable(app, app->bck.id, app->ext.id, app->ext.fd);
    }
}

void updateExpeditionTable(AppNode *app, char *dest_id, char *neigh_id, int neigh_fd)
{
    strcpy(app->expeditionTable[atoi(dest_id)].id, neigh_id);
    app->expeditionTable[atoi(dest_id)].fd = neigh_fd;
}

void handleQUERYmessage(AppNode *app, NODE node, char *cmd, char *token)
{
    char dest_id[3], orig_id[3], name[100];
    char buffer[MAX_BUFFER_SIZE] = "\0";

    if (strcmp(cmd, "QUERY") == 0)
    {
        if (sscanf(token, "QUERY %s %s %s", dest_id, orig_id, name) != 3)
        {
            printf("Bad response from server\n");
        }
        updateExpeditionTable(app, orig_id, node.id, node.fd);
        if (strcmp(app->self.id, dest_id) == 0) // mensagem de query chegou ao destino
        {
            if (searchContentOnList(app, name) > 0)
                sprintf(buffer, "CONTENT %s %s %s\n", orig_id, dest_id, name);
            else
                sprintf(buffer, "NOCONTENT %s %s %s\n", orig_id, dest_id, name);

            if (writeTcp(app->expeditionTable[atoi(orig_id)].fd, buffer) < 0)
            {
                printf("Cant send Content message\n");
                exit(1);
            }
        }
        else // propagar queries pelos vizinhos
        {
            sprintf(buffer, "QUERY %s %s %s\n", dest_id, orig_id, name);
            shareQUERYmessages(app, node, buffer, dest_id);
        }
    }
}

void shareQUERYmessages(AppNode *app, NODE node, char *buffer, char *dest_id)
{
    if (app->expeditionTable[atoi(dest_id)].fd == 0) // ainda n possui o no destino da tabela de expediçao
    {
        if (strcmp(node.id, app->ext.id) != 0)
        {
            if (writeTcp(app->ext.fd, buffer) < 0)
            {
                printf("Can't write when sending queries\n");
                exit(1);
            }
        }
        for (int i = 0; i < app->interns.numIntr; i++)
        {
            if (strcmp(node.id, app->interns.intr[i].id) != 0)
            {
                if (writeTcp(app->interns.intr[i].fd, buffer) < 0)
                {
                    printf("Can't write when sending queries\n");
                    exit(1);
                }
            }
        }
    }
    else
    {
        if (writeTcp(app->expeditionTable[atoi(dest_id)].fd, buffer) < 0)
        {
            printf("Can't write when someone is trying to connect\n");
            exit(1);
        }
    }
}

void externCommunication(AppNode *app, char *buffer)
{
    char buffer_cpy[MAX_BUFFER_SIZE] = "\0", *token, cmd[24];
    strcpy(buffer_cpy, buffer);
    token = strtok(buffer_cpy, "\n");
    while (token != NULL)
    {
        strcpy(cmd, token);
        cmd[strcspn(cmd, " ")] = '\0';
        handleEXTmessage(app, cmd, token);
        handleQUERYmessage(app, app->ext, cmd, token);
        handleCONTENTmessage(app, cmd, token);
        token = advancePointer(token);
        token = strtok(token, "\n");
    }
}

void handleCONTENTmessage(AppNode *app, char *cmd, char *token)
{
    char dest_id[3], orig_id[3], name[100];
    char buffer[MAX_BUFFER_SIZE] = "\0";

    if (strcmp(cmd, "CONTENT") == 0)
    {
        if (sscanf(token, "CONTENT %s %s %s", dest_id, orig_id, name) != 3)
        {
            printf("Cant read CONTENT msg\n");
        }
        if (strcmp(app->self.id, dest_id) == 0) // mensagem de content chegou ao destino
        {
            printf("%s is present on %s\n", name, orig_id);
        }
        else // enviar msg de content pelo vizinho que possui a socket para comunicar com o nó destino
        {
            sprintf(buffer, "CONTENT %s %s %s\n", dest_id, orig_id, name);
            if (writeTcp(app->expeditionTable[atoi(dest_id)].fd, buffer) < 0)
            {
                printf("Cant send Content message\n");
                exit(1);
            }
        }
    }
    else if (strcmp(cmd, "NOCONTENT") == 0)
    {
        if (sscanf(token, "NOCONTENT %s %s %s", dest_id, orig_id, name) != 3)
        {
            printf("Cant read NOCONTENT msg\n");
        }
        if (strcmp(app->self.id, dest_id) == 0) // mensagem de content chegou ao destino
        {
            printf("%s is not present on %s\n", name, orig_id);
        }
        else // enviar msg de no_content pelo vizinho que possui a socket para comunicar com o nó destino
        {
            sprintf(buffer, "NOCONTENT %s %s %s\n", dest_id, orig_id, name);
            if (writeTcp(app->expeditionTable[atoi(dest_id)].fd, buffer) < 0)
            {
                printf("Cant send NoContent message\n");
                exit(1);
            }
        }
    }
}

void internCommunication(AppNode *app, NODE node, char *buffer)
{
    char buffer_cpy[MAX_BUFFER_SIZE] = "\0", *token, cmd[24];
    strcpy(buffer_cpy, buffer);
    token = strtok(buffer_cpy, "\n");
    while (token != NULL)
    {
        strcpy(cmd, token);
        cmd[strcspn(cmd, " ")] = '\0';
        handleQUERYmessage(app, node, cmd, token);
        handleCONTENTmessage(app, cmd, token);
        token = advancePointer(token);
        token = strtok(token, "\n");
    }
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
        if (writeTcp(app->ext.fd, buffer) < 0)
        {
            printf("Can't write when I'm trying to connect\n");
            exit(1);
        }
        FD_SET(app->ext.fd, currentSockets);
    }
    else
    {
        printf("TCP ERROR!!\n");
        memmove(&app->ext, &app->self, sizeof(NODE)); // couldnt connect to ext node
    }
}

void chooseRandomNodeToConnect(char *buffer)
{
    char buffer_cpy[MAX_BUFFER_SIZE];
    char *token;
    int counter = 0, i, num;
    srand(time(0));

    memset(buffer_cpy, 0, MAX_BUFFER_SIZE);
    strcpy(buffer_cpy, buffer);
    token = strtok(buffer_cpy, "\n");
    token = strtok(NULL, "\n");
    while (token != NULL)
    {
        counter++;
        token = strtok(NULL, "\n");
    }

    if (counter == 0) // lista de nos na rede vazia
    {
        return;
    }

    token = strtok(buffer, "\n");
    token = strtok(NULL, "\n");

    num = (rand() % counter);
    for (i = 0; i < counter - 1; i++)
    {
        if (i == num)
            break;
        token = strtok(NULL, "\n");
    }
    strcpy(buffer, token);
}

void joinCommand(AppNode *app, fd_set *currentSockets, char *buffer, char *regIP, char *regUDP, char *net)
{
    char serverResponse[16];
    sprintf(serverResponse, "NODESLIST %s\n", net);
    memmove(&app->ext, &app->self, sizeof(NODE));
    memmove(&app->bck, &app->self, sizeof(NODE));
    memset(buffer, 0, MAX_BUFFER_SIZE);
    sprintf(buffer, "NODES %s", net);
    udpClient(buffer, regIP, regUDP, net); // gets net nodes
    chooseRandomNodeToConnect(buffer);

    if (strcmp(buffer, serverResponse) == 0) // rede esta vazia
    {
        regNetwork(app, buffer, currentSockets, regIP, regUDP, net);
    }
    else
    {
        if (sscanf(buffer, "%s %s %s", app->ext.id, app->ext.ip, app->ext.port) == 3)
        {
            // memmove(&app.bck, &app.ext, sizeof(NODE));
            if ((app->ext.fd = connectTcpClient(app)) > 0) // join network by connecting to another node
            {
                memset(buffer, 0, MAX_BUFFER_SIZE);
                sprintf(buffer, "NEW %s %s %s\n", app->self.id, app->self.ip, app->self.port);
                printf("sent: %s\n", buffer);
                if (writeTcp(app->ext.fd, buffer) < 0)
                {
                    printf("Can't write when I'm trying to connect\n");
                    exit(1);
                }
                // adicionar externo aos vizinhos
                updateExpeditionTable(app, app->ext.id, app->ext.id, app->ext.fd);
                regNetwork(app, buffer, currentSockets, regIP, regUDP, net);
                FD_SET(app->ext.fd, currentSockets);
            }
            else
            {
                printf("TCP ERROR!!\n");
                memmove(&app->ext, &app->self, sizeof(NODE)); // couldnt connect to ext node
            }
        }
    }
}

int searchContentOnList(AppNode *app, char *name)
{
    LinkedList *aux;
    aux = app->self.contentList;
    while (aux != NULL)
    {
        if (strcmp(aux->contentName, name) == 0)
            return 1;
        aux = aux->next;
    }
    return 0;
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

void deleteCommand(AppNode *app, char *name)
{
    LinkedList *ptr, *aux, *head;
    head = app->self.contentList;
    ptr = app->self.contentList;
    for (aux = ptr; aux != NULL; aux = aux->next)
    {
        if (strcmp(aux->contentName, name) == 0)
        {
            if (aux == head) // Head of the LinkedList
            {
                app->self.contentList = aux->next;
                free(ptr);
                return;
            }
            else
            {
                ptr->next = aux->next;
                free(aux);
                return;
            }
        }
        ptr = aux;
    }
}

void createCommand(AppNode *app, char *name)
{
    if (searchContentOnList(app, name) == 0)
    {
        LinkedList *new_node = (LinkedList *)malloc(sizeof(LinkedList)); // allocate memory for LinkedList struct
        if (new_node == NULL)
        {
            printf("Malloc went wrong\n");
            return;
        }
        strcpy(new_node->contentName, name);
        new_node->next = NULL; // set the next pointer to NULL

        if (app->self.contentList == NULL)
        {
            app->self.contentList = new_node;
            return;
        }

        LinkedList *list = app->self.contentList; // use a separate pointer to iterate over the list
        while (list->next != NULL)
        {
            list = list->next; // advance to the next node in the list
        }

        list->next = new_node; // add the new node to the end of the list
    }
    else
        printf("Name's already on Content List\n");
}

void showNamesCommand(AppNode *app)
{
    LinkedList *aux;
    printf("---- CONTENT LIST ----\n");
    for (aux = app->self.contentList; aux != NULL; aux = aux->next)
    {
        printf("%s\n", aux->contentName);
    }
}

void getCommand(AppNode *app, char *dest, char *name)
{
    char buffer[MAX_BUFFER_SIZE] = "\0";
    sprintf(buffer, "QUERY %s %s %s\n", dest, app->self.id, name);
    if (app->expeditionTable[atoi(dest)].fd == 0) // ainda n possui a melhor rota para chegar ao destino
    {
        if (writeTcp(app->ext.fd, buffer) < 0)
        {
            printf("Cant send QUERY msg to extern\n");
        }
        writeMessageToInterns(app, buffer);
    }
    else
    {
        if (writeTcp(app->expeditionTable[atoi(dest)].fd, buffer) < 0)
        {
            printf("Cant send QUERY msg\n");
        }
    }
}

void showRoutingCommand(AppNode *app)
{
    char id[3];
    printf("---- Expedition Table ----\n");
    for (int i = 0; i < 99; i++)
    {
        if (app->expeditionTable[i].fd > 0)
        {
            sprintf(id, "%02d", i);
            printf("%s -> %s\n", id, app->expeditionTable[i].id);
        }
    }
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
    case CREATE:
        createCommand(app, name);
        break;
    case DELETE:
        deleteCommand(app, name);
        break;
    case GET:
        getCommand(app, dest, name);
        break;
    case SHOW_TOPOLOGY:
        showTopologyCommand(app);
        break;
    case SHOW_NAMES:
        showNamesCommand(app);
        break;
    case SHOW_ROUTING:
        showRoutingCommand(app);
        break;
    case LEAVE:
        leaveCommand(app, buffer, currentSockets, regIP, regUDP, net);
        break;
    case EXIT:
        FD_CLR(app->self.fd, currentSockets);
        close(app->self.fd);
        freeContentList(app);
        exit(0);
    default:
        printf("unknown command\n");
        break;
    }
}

void freeContentList(AppNode *app)
{
    LinkedList *aux, *ptr;
    aux = ptr = app->self.contentList;
    while (aux != NULL)
    {
        aux = aux->next;
        free(ptr);
    }
}

void init(AppNode *app, NodeQueue *queue, char *regIP, char *regUDP, char **argv)
{
    if (validateCommandLine(argv) < 0)
    {
        printf("wrong inputs ./cot [IP] [TCP] [regIP] [regUDP]\n");
        exit(1);
    }
    memset(app, 0, sizeof(struct AppNode));
    memset(queue, 0, sizeof(struct NodeQueue));
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

int readTcp(int fd, char *buffer)
{
    char *buffer_cpy;
    ssize_t n;
    memset(buffer, 0, MAX_BUFFER_SIZE);
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

int writeTcp(int fd, char *buffer)
{
    if (write(fd, buffer, strlen(buffer)) < 0)
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
        "show",
        "leave",
        "exit",
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
        if ((token = strtok(NULL, " \n\r\t")) == NULL)
            return -1;
        if (strcmp(token, "topology") == 0)
            *cmd = SHOW_TOPOLOGY;
        else if (strcmp(token, "names") == 0)
            *cmd = SHOW_NAMES;
        else if (strcmp(token, "routing") == 0)
            *cmd = SHOW_ROUTING;
        else
            *cmd = -1;
        break;
    case 6:
        *cmd = LEAVE;
        break;
    case 7:
        *cmd = EXIT;
        exit(1);
        break;
    default:
        *cmd = -1;
        break;
    }
    return 0;
}
