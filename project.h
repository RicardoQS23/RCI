#ifndef _PROJECT_
#define _PROJECT_

#define MAX_BUFFER_SIZE 1024

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

/**
 * @brief Each element of this list holds the name of a singular "content" of a node
 */
typedef struct LinkedList
{
    char contentName[100];
    struct LinkedList *next;
} LinkedList;

/**
 * @brief Definition of a socket, with each socket having its own buffer wich will store all information sent to it
 */
typedef struct SOCKET
{
    int fd;
    char buffer[MAX_BUFFER_SIZE];
}SOCKET;

/**
 * @brief Definition of a node 
 */
typedef struct NODE
{
    SOCKET socket;
    char ip[16];
    char port[6];
    char id[3];
} NODE;

/**
 * @brief Definition of the Queue responsible for holding all the nodes that are connected to a determined node, but
 * are yet to send the NEW command. A node will only be removed from the queue once the NEW command is recieved.
 */
typedef struct NodeQueue
{
    NODE queue[5];
    int numNodesInQueue;
} NodeQueue;

/**
 * @brief Here, the "intr" array holds all interns of a node. 
 */
typedef struct INTR
{
    NODE intr[98];
    int numIntr;
} INTR;

/**
 * @brief Definition of the Expedition Table. The expedition table of a node holds the direction a signal must follow
 * to get to another node
 */
typedef struct ExpeditionTable
{
    int fd; //neighbour fd
    char id[3]; //neighbour id
} ExpeditionTable;

/**
 * @brief Defenition of the Application
 */
typedef struct AppNode
{
    NODE self;
    NODE bck;
    NODE ext;
    INTR interns;
    LinkedList *contentList;
    ExpeditionTable expeditionTable[100];
} AppNode;

/**
 * @brief All commands to be handlled 
 */
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
void joinCommand(AppNode *app, NODE *temporaryExtern, fd_set *currentSockets, char *regIP, char *regUDP, char *net);
void djoinCommand(AppNode *app, fd_set *currentSockets, char *bootID, char *bootIP, char *bootTCP);
void leaveCommand(AppNode *app, fd_set *currentSockets, char *regIP, char *regUDP, char *net);
void createCommand(AppNode *app, char *name);
void deleteCommand(AppNode *app, char *name);
void getCommand(AppNode *app, char *dest, char *name);
void showTopologyCommand(AppNode *app);
void showNamesCommand(AppNode *app);
void showRoutingCommand(AppNode *app);
void clearNamesCommand(AppNode *app);
void clearRoutingCommand(AppNode *app);
void loadCommand(AppNode *app, char *fileName);
void commandMultiplexer(AppNode *app, NODE *temporaryExtern, enum commands cmd, fd_set *currentSockets, char *buffer, char *bootIP,char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName);

                                        /*  VALIDATION RELATED FUNCTIONS */
void init(AppNode *app, NodeQueue *queue, NODE *temporaryExtern, char *regIP, char *regUDP, char **argv);
int validateUserInput(AppNode *app, enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *fileName);
int validateCommandLine(char **cmdLine);
int validate_number(char *str);
int validate_ip(char *ip);
int compare_cmd(char *cmdLine);
int countLFchars(char *buffer);
char *advancePointer(char *token);

                                        /*  UDP COMMUNICATION RELATED FUNCTIONS */
int chooseRandomNodeToConnect(char *buffer, char *my_id);
void udpClient(char *buffer, char *ip, char *port, char *net);
void regNetwork(AppNode *app, fd_set *currentSockets, char *regIP, char *regUDP, char *net);
void unregNetwork(AppNode *app, fd_set *currentSockets, char *regIP, char *regUDP, char *net);

                                        /* TCP COMMUNICATION RELATED FUNCTIONS */
int openTcpServer(AppNode *app);
int connectTcpClient(AppNode *app, NODE *temporaryExtern);
int acceptTcpServer(AppNode *app);
void acceptNeighbourConnection(AppNode *app, NodeQueue *queue, fd_set *currentSockets);
void handleCommunication(AppNode *app, NODE *node);
void queueCommunication(AppNode *app, NodeQueue *queue, fd_set *currentSockets, int pos);
void temporaryExternCommunication(AppNode *app, NODE *temporaryExtern, fd_set *currentSockets);
void closedExtConnection(AppNode *app, NODE *temporaryExtern, fd_set *currentSockets);
void closedIntConnection(AppNode *app, fd_set *currentSockets, int i);
int readTcp(SOCKET *socket);
int writeTcp(SOCKET socket);
void writeMessageToInterns(AppNode *app, char *buffer);
void promoteInternToExtern(AppNode *app);
void connectToBackup(AppNode *app, NODE *temporaryExtern, fd_set *currentSockets);

                                        /* NODE QUEUE RELATED FUNCTIONS */
void cleanQueue(NodeQueue *queue, fd_set *currentSockets);
void popQueue(NodeQueue *queue, fd_set *currentSockets, int pos);
void promoteQueueToIntern(AppNode *app, NodeQueue *queue, fd_set *currentSockets, int pos);
void promoteTemporaryToExtern(AppNode *app, NODE *temporaryExtern);
void resetTemporaryExtern(NODE *temporaryExtern, fd_set *currentSockets);

                                        /* INTERRUPTION HANDLING RELATED FUNCTIONS */
void handleInterruptions(AppNode *app, NodeQueue *queue, NODE *temporaryExtern, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName);
void handleUserInputInterruption(AppNode *app, NODE *temporaryExtern, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName);
void handleServerInterruption(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets);
void handleExtInterruption(AppNode *app, NODE *temporaryExtern, fd_set *readSockets, fd_set *currentSockets);
void handleInternInterruptions(AppNode *app, fd_set *readSockets, fd_set *currentSockets);
void handleQueueInterruptions(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets);
void handleTemporaryExternInterruption(AppNode *app, NODE *temporaryNode, fd_set *readSockets, fd_set *currentSockets);

                                        /* EXPEDITION TABLE AND CONTENT LIST RELATED FUNCTIONS */
void updateExpeditionTable(AppNode *app, char *dest_id, char *neigh_id, int neigh_fd);
void clearExpeditionTable(AppNode *app);
int searchContentOnList(AppNode *app, char *name);
void freeContentList(AppNode *app);

                                        /* MESSAGE HANDLING RELATED FUNCTIONS */
int handleNEWmessage(AppNode *app, NodeQueue *queue, fd_set *currentSockets, int pos, char *token);
int handleFirstEXTmessage(AppNode *app, NODE *temporaryExtern, char *token);
void handleEXTmessage(AppNode *app, char *token);
void handleQUERYmessage(AppNode *app, NODE node, char *token);
void handleCONTENTmessage(AppNode *app, NODE node, char *token);
void handleNOCONTENTmessage(AppNode *app, NODE node, char *token);
void handleWITHDRAWmessage(AppNode *app, NODE node, char *token);
void shareQUERYmessages(AppNode *app, NODE node, char *buffer, char *dest_id);
void shareWITHDRAWmessages(AppNode *app, NODE node, char *token);
#endif