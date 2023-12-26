#include "project.h"

/**
 * @brief This function opens a TCP server
 */
int openTcpServer(AppNode *app)
{
    int fd;
    struct addrinfo hints, *res;

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

/**
 * @brief This function is used to connect to a client in the Tcp server
 */
int connectTcpClient(NODE *temporaryExtern)
{
    int fd;
    struct addrinfo hints, *res;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(temporaryExtern->ip, temporaryExtern->port, &hints, &res) != 0)
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

/**
 * @brief This function creates a fd for the conection established
 */
int acceptTcpServer(AppNode *app)
{
    int fd;
    struct sockaddr_in addr;
    socklen_t addrlen;

    addrlen = sizeof(addr);
    if ((fd = accept(app->self.socket.fd, (struct sockaddr *)&addr, &addrlen)) == -1)
        return -1;

    return fd;
}

/**
 * @brief This function is responsible for accpeting the connection requests from "clients"
 */
void acceptNeighbourConnection(AppNode *app, NodeQueue *queue, fd_set *currentSockets)
{
    // TODO antes de atribuir titulo de externo, assumir q é interno e mete-lo numa queue
    //      assim que ele receber msg do outro lado, tira-lo da queue e ascende-lo a externo
    if ((queue->queue[queue->numNodesInQueue].socket.fd = acceptTcpServer(app)) > 0)
    {
        FD_SET(queue->queue[queue->numNodesInQueue].socket.fd, currentSockets);
        queue->numNodesInQueue++;
    }
    else
    {
        printf("Couldn't accept TCP connection\n");
    }
}

/**
 * @brief This function consumes the node's socket buffer, separating each completed message by the '\n' and consequently,
 * handling them with the respective function. The remaining incomplete messages are then stored in the socket until another instance of the readTcp comletes them.
 */
void handleCommunication(AppNode *app, NODE *node)
{
    char buffer_cpy[MAX_BUFFER_SIZE] = "\0", *token, cmd[24];
    int numCompletedMsg, numMsgRead = 0;
    numCompletedMsg = countLFchars(node->socket.buffer);
    if (numCompletedMsg == 0)
        return;

    strcpy(buffer_cpy, node->socket.buffer);
    token = strtok(buffer_cpy, "\n");

    while (numMsgRead < numCompletedMsg)
    {
        strcpy(cmd, token);
        cmd[strcspn(cmd, " ")] = '\0';
        if (strcmp(cmd, "EXTERN") == 0)
            handleEXTmessage(app, token);
        else if (strcmp(cmd, "QUERY") == 0)
            handleQUERYmessage(app, *node, token);
        else if (strcmp(cmd, "CONTENT") == 0)
            handleCONTENTmessage(app, *node, token);
        else if (strcmp(cmd, "NOCONTENT") == 0)
            handleNOCONTENTmessage(app, *node, token);
        else if (strcmp(cmd, "WITHDRAW") == 0)
            handleWITHDRAWmessage(app, *node, token);

        token = advancePointer(token);
        token = strtok(token, "\n");
        numMsgRead++;
    }
    memset(node->socket.buffer, 0, MAX_BUFFER_SIZE);
    if (token != NULL)
        strcpy(node->socket.buffer, token);
}

/**
 * @brief Handles comunication for the nodes that are still on the queue
 */
void queueCommunication(AppNode *app, NODE *temporaryExtern, NodeQueue *queue, fd_set *currentSockets, int pos)
{
    char buffer_cpy[MAX_BUFFER_SIZE] = "\0", *token, cmd[24];
    int numCompletedMsg, numMsgRead = 0, promotedFlag = 0;
    numCompletedMsg = countLFchars(queue->queue[pos].socket.buffer);
    if (numCompletedMsg == 0)
        return;

    strcpy(buffer_cpy, queue->queue[pos].socket.buffer);
    token = strtok(buffer_cpy, "\n");

    while (numMsgRead < numCompletedMsg)
    {
        strcpy(cmd, token);
        cmd[strcspn(cmd, " ")] = '\0';
        if (strcmp(cmd, "NEW") == 0)
            promotedFlag = handleNEWmessage(app, queue, temporaryExtern, currentSockets, pos, token);
        token = advancePointer(token);
        token = strtok(token, "\n");
        numMsgRead++;
    }
    if (promotedFlag == 1)
    {
        memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);
        if (token != NULL)
            strcpy(app->ext.socket.buffer, token);
    }
    else if (promotedFlag == 0)
    {
        memset(app->interns.intr[app->interns.numIntr - 1].socket.buffer, 0, MAX_BUFFER_SIZE);
        if (token != NULL)
            strcpy(app->interns.intr[app->interns.numIntr - 1].socket.buffer, token);
    }
    else
    {
        memset(queue->queue[pos].socket.buffer, 0, MAX_BUFFER_SIZE);
        if (token != NULL)
            strcpy(queue->queue[pos].socket.buffer, token);
    }
}

/**
 * @brief This function handles the first 'EXTERN' msg after a connect operation. If there is no connection made the temporary extern node is discarded
 */
void temporaryExternCommunication(AppNode *app, NODE *temporaryExtern)
{
    char buffer_cpy[MAX_BUFFER_SIZE] = "\0", *token, cmd[24];
    int numCompletedMsg, numMsgRead = 0, promotedFlag = 0;
    numCompletedMsg = countLFchars(temporaryExtern->socket.buffer);
    if (numCompletedMsg == 0)
        return;

    strcpy(buffer_cpy, temporaryExtern->socket.buffer);
    token = strtok(buffer_cpy, "\n");
    while (numMsgRead < numCompletedMsg)
    {
        strcpy(cmd, token);
        cmd[strcspn(cmd, " ")] = '\0';
        if (strcmp(cmd, "EXTERN") == 0)
            promotedFlag = handleFirstEXTmessage(app, temporaryExtern, token);
        token = advancePointer(token);
        token = strtok(token, "\n");
        numMsgRead++;
    }
    if (promotedFlag)
    {
        memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);
        if (token != NULL)
            strcpy(app->ext.socket.buffer, token);
    }
    else
    {
        memset(temporaryExtern->socket.buffer, 0, MAX_BUFFER_SIZE);
        if (token != NULL)
            strcpy(temporaryExtern->socket.buffer, token);
    }
}

/**
 * @brief Handles the exit of the extern node
 */
void closedExtConnection(AppNode *app, NODE *temporaryExtern, fd_set *currentSockets)
{
    char buffer[64] = "\0", id[3];
    memset(buffer, 0, 64);
    sprintf(buffer, "WITHDRAW %s\n", app->ext.id);

    updateExpeditionTable(app, app->ext.id, "-1", 0);
    for (int i = 0; i < 100; i++)
    {
        if (strcmp(app->expeditionTable[i].id, app->ext.id) == 0)
        {
            sprintf(id, "%02d", i);
            updateExpeditionTable(app, id, "-1", 0);
        }
    }

    writeMessageToInterns(app, temporaryExtern, currentSockets, buffer);

    if (strcmp(app->self.id, app->bck.id) == 0) // sou ancora
    {
        if (app->interns.numIntr > 0) // escolhe um dos internos para ser o seu novo externo
        {
            // promove interno a externo e retira-o da lista de internos
            FD_CLR(app->ext.socket.fd, currentSockets);
            promoteInternToExtern(app, temporaryExtern, currentSockets);
        }
        else // ficou sozinho na rede
        {
            FD_CLR(app->ext.socket.fd, currentSockets);
            close(app->ext.socket.fd);
            memmove(&app->ext, &app->self, sizeof(NODE));
        }
    }
    else // nao é ancora
    {
        connectToBackup(app, temporaryExtern, currentSockets);
    }
}

/**
 * @brief This function reacts to the exit of an intern node
 */
void closedIntConnection(AppNode *app, NODE *temporaryExtern, fd_set *currentSockets, int i)
{
    char buffer[64] = "\0", id[3];
    FD_CLR(app->interns.intr[i].socket.fd, currentSockets);
    close(app->interns.intr[i].socket.fd);

    updateExpeditionTable(app, app->interns.intr[i].id, "-1", 0);
    for (int i = 0; i < 100; i++)
    {
        if (strcmp(app->expeditionTable[i].id, app->interns.intr[i].id) == 0)
        {
            sprintf(id, "%02d", i);
            updateExpeditionTable(app, id, "-1", 0);
        }
    }

    sprintf(buffer, "WITHDRAW %s\n", app->interns.intr[i].id);
    strcpy(app->ext.socket.buffer, buffer);
    if (writeTcp(app->ext.socket) < 0)
    {
        printf("Can't send WITHDRAW msg to extern\n");
        closedExtConnection(app, temporaryExtern, currentSockets);
    }
    memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);

    memmove(&app->interns.intr[i], &app->interns.intr[app->interns.numIntr - 1], sizeof(NODE));
    memset(&app->interns.intr[app->interns.numIntr - 1], 0, sizeof(NODE));
    app->interns.numIntr--;
    writeMessageToInterns(app, temporaryExtern, currentSockets, buffer);
}

/**
 * @brief In this function, we read the information sent to the node's socket and then we concatenate it to the socket's buffer
 */
int readTcp(SOCKET *socket)
{
    ssize_t n;
    char *buffer_cpy = (char *)calloc(MAX_BUFFER_SIZE, sizeof(char));
    if (buffer_cpy == NULL)
    {
        printf("Error in calloc\n");
        return 0;
    }

    if ((n = read((*socket).fd, buffer_cpy, MAX_BUFFER_SIZE)) <= 0)
    {
        close((*socket).fd);
        free(buffer_cpy);
        return -1;
    }
    else
    {
        if (strlen(socket->buffer) + strlen(buffer_cpy) > MAX_BUFFER_SIZE)
            memset(socket->buffer, 0, MAX_BUFFER_SIZE);
        strcat(socket->buffer, buffer_cpy);
        //printf("\033[34mrecv: %s\033[0m\n", socket->buffer);
    }
    free(buffer_cpy);
    return 0;
}

/**
 * @brief In order to garantee that the message is sent in its inteirity,
 *  we keep writing while the number of bytes of the messages hasn't reached
 *  the total number of bytes of the message
 */
int writeTcp(SOCKET socket)
{
    ssize_t n_ToSend, n_Sent, n_totalSent = 0;
    n_ToSend = strlen(socket.buffer);
    while (n_totalSent < n_ToSend)
    {
        n_Sent = write(socket.fd, socket.buffer, strlen(socket.buffer));
        //printf("\033[35msent: %s\033[0m\n", socket.buffer);
        if (n_Sent <= 0)
        {
            close(socket.fd);
            return -1;
        }
        n_totalSent += n_Sent;
    }

    return 0;
}

/**
 * @brief This functions writes the message received from the buffer into the intern's respective socket buffer
 */
void writeMessageToInterns(AppNode *app, NODE *temporaryExtern, fd_set *currentSockets, char *buffer)
{
    for (int i = 0; i < app->interns.numIntr; i++)
    {
        strcpy(app->interns.intr[i].socket.buffer, buffer);
        if (writeTcp(app->interns.intr[i].socket) < 0)
        {
            printf("Can't write to intern\n");
            closedIntConnection(app, temporaryExtern, currentSockets, i);
        }
        memset(app->interns.intr[i].socket.buffer, 0, MAX_BUFFER_SIZE);
    }
}

/**
 * @brief This function promotes a node's intern to it's extern, when it's previous extern has left the net
 */
void promoteInternToExtern(AppNode *app, NODE *temporaryExtern, fd_set *currentSockets)
{
    char buffer[MAX_BUFFER_SIZE] = "\0";
    memmove(&app->ext, &app->interns.intr[0], sizeof(NODE));
    memmove(&app->interns.intr[0], &app->interns.intr[app->interns.numIntr - 1], sizeof(NODE));
    memset(&app->interns.intr[app->interns.numIntr - 1], 0, sizeof(NODE));
    app->interns.numIntr--;

    sprintf(app->ext.socket.buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
    strcpy(buffer, app->ext.socket.buffer);
    if (writeTcp(app->ext.socket) < 0)
    {
        printf("Can't write when I'm trying to connect\n");
        closedExtConnection(app, temporaryExtern, currentSockets);
    }

    memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);
    writeMessageToInterns(app, temporaryExtern, currentSockets, buffer);
}

/**
 * @brief When a node's extern is disconnected, it's backup is turned into the new extern
 */
void connectToBackup(AppNode *app, NODE *temporaryExtern, fd_set *currentSockets)
{
    char buffer[64] = "\0";
    // altera o novo temporary extern para o backup
    memmove(&app->ext, &app->self, sizeof(NODE));
    strcpy(temporaryExtern->id, app->bck.id);
    strcpy(temporaryExtern->ip, app->bck.ip);
    strcpy(temporaryExtern->port, app->bck.port);

    if ((temporaryExtern->socket.fd = connectTcpClient(temporaryExtern)) > 0) // join network by connecting to another node
    {
        FD_SET(temporaryExtern->socket.fd, currentSockets); // sucessfully connected to ext node
        sprintf(temporaryExtern->socket.buffer, "NEW %s %s %s\n", app->self.id, app->self.ip, app->self.port);
        if (writeTcp(temporaryExtern->socket) < 0)
        {
            printf("Can't write when I'm trying to connect\n");
            closedExtConnection(app, temporaryExtern, currentSockets);
        }
        memset(temporaryExtern->socket.buffer, 0, MAX_BUFFER_SIZE);

        if (app->interns.numIntr > 0) // tem nos intr e comunica com todos para atualizar os seus nos de backup
        {
            memset(buffer, 0, 64);
            sprintf(buffer, "EXTERN %s %s %s\n", temporaryExtern->id, temporaryExtern->ip, temporaryExtern->port);
            writeMessageToInterns(app, temporaryExtern, currentSockets, buffer);
        }
        //updateExpeditionTable(app, temporaryExtern->id, temporaryExtern->id, temporaryExtern->socket.fd);
    }
    else
    {
        printf("TCP ERROR!!\n");
        memmove(temporaryExtern, &app->self, sizeof(NODE)); // couldnt connect to ext node
    }
}
