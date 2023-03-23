#ifndef _PROJECT_
#define _PROJECT_

#define MAX_BUFFER_SIZE 256

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>

typedef struct LinkedList
{
    char contentName[100];
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
    ExpeditionTable expeditionTable[100];
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
    EXIT,
    CLEAR_NAMES,
    CLEAR_ROUTING,
    LOAD
};

                                        /*  USER INPUT RELATED FUNCTIONS */
void joinCommand(AppNode *app, fd_set *currentSockets, char *buffer, char *regIP, char *regUDP, char *net);
void djoinCommand(AppNode *app, fd_set *currentSockets, char *buffer, char *bootID, char *bootIP, char *bootTCP);
void leaveCommand(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net);
void createCommand(AppNode *app, char *name);
void deleteCommand(AppNode *app, char *name);
void getCommand(AppNode *app, char *dest, char *name);
void showTopologyCommand(AppNode *app);
void showNamesCommand(AppNode *app);
void showRoutingCommand(AppNode *app);
void clearNamesCommand(AppNode *app);
void clearRoutingCommand(AppNode *app);
void loadCommand(AppNode *app, char *fileName, char *buffer);
void commandMultiplexer(AppNode *app, char *buffer, enum commands cmd, fd_set *currentSockets, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName);

                                        /*  VALIDATION RELATED FUNCTIONS */
void init(AppNode *app, NodeQueue *queue, char *regIP, char *regUDP, char **argv);
int validateUserInput(enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *fileName, AppNode *app);
int validateCommandLine(char **cmdLine);
int validate_number(char *str);
int validate_ip(char *ip);
int compare_cmd(char *cmdLine);
char *advancePointer(char *token);

                                        /*  UDP COMMUNICATION RELATED FUNCTIONS */
int chooseRandomNodeToConnect(char *buffer, char *my_id);
void udpClient(char *buffer, char *ip, char *port, char *net);
void regNetwork(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net);
void unregNetwork(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net);

                                        /* TCP COMMUNICATION RELATED FUNCTIONS */
int openTcpServer(AppNode *app);
int connectTcpClient(AppNode *app);
int acceptTcpServer(AppNode *app);
void acceptNeighbourConnection(AppNode *app, NodeQueue *queue, fd_set *currentSockets);
void externCommunication(AppNode *app, char *buffer);
void internCommunication(AppNode *app, NODE node, char *buffer);
void closedExtConnection(AppNode *app, char *buffer, fd_set *currentSockets);
void closedIntConnection(AppNode *app, fd_set *currentSockets, int i);
int readTcp(int fd, char *buffer);
int writeTcp(int fd, char *buffer);
void writeMessageToInterns(AppNode *app, char *buffer);
void promoteInternToExtern(AppNode *app, char *buffer);
void connectToBackup(AppNode *app, char *buffer, fd_set *currentSockets);

                                        /* NODE QUEUE RELATED FUNCTIONS */
void cleanQueue(NodeQueue *queue, fd_set *currentSockets);
void popQueue(NodeQueue *queue, fd_set *currentSockets, int pos);
void promoteQueueToIntern(AppNode *app, NodeQueue *queue, fd_set *currentSockets, int pos);

                                        /* INTERRUPTION HANDLING RELATED FUNCTIONS */
void handleInterruptions(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName);
void handleUserInputInterruption(AppNode *app, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName);
void handleServerInterruption(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets);
void handleExtInterruption(AppNode *app, char *buffer, fd_set *readSockets, fd_set *currentSockets);
void handleInternInterruptions(AppNode *app, fd_set *readSockets, fd_set *currentSockets, char *buffer);
void handleQueueInterruptions(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets, char *buffer);

                                        /* EXPEDITION TABLE AND CONTENT LIST RELATED FUNCTIONS */
void updateExpeditionTable(AppNode *app, char *dest_id, char *neigh_id, int neigh_fd);
void clearExpeditionTable(AppNode *app);
int searchContentOnList(AppNode *app, char *name);
void freeContentList(AppNode *app);

                                        /* MESSAGE HANDLING RELATED FUNCTIONS */
void handleEXTmessage(AppNode *app, char *cmd, char *token);
void handleQUERYmessage(AppNode *app, NODE node, char *cmd, char *token);
void handleCONTENTmessage(AppNode *app, NODE node, char *cmd, char *token);
void handleWITHDRAWmessage(AppNode *app, NODE node, char *cmd, char *buffer);
void shareQUERYmessages(AppNode *app, NODE node, char *buffer, char *dest_id);
void shareWITHDRAWmessages(AppNode *app, NODE node, char *buffer);
#endif