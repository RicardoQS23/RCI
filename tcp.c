#include "project.h"

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

int connectTcpClient(AppNode *app)
{
    int fd;
    struct addrinfo hints, *res;

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

void handleCommunication(AppNode *app, NODE *node)
{
    char buffer_cpy[MAX_BUFFER_SIZE] = "\0", *token, cmd[24];
    int numCompletedMsg , numMsgRead = 0;
    numCompletedMsg = countLFchars(node->socket.buffer);
    if(numCompletedMsg == 0)
        return;
    
    strcpy(buffer_cpy, node->socket.buffer);
    token = strtok(buffer_cpy, "\n");

    while (numMsgRead < numCompletedMsg)
    {
        strcpy(cmd, token);
        cmd[strcspn(cmd, " ")] = '\0';
        handleEXTmessage(app, cmd, token);
        handleQUERYmessage(app, *node, cmd, token);
        handleCONTENTmessage(app, *node, cmd, token);
        handleWITHDRAWmessage(app, *node, cmd, token);
        token = advancePointer(token);
        token = strtok(token, "\n");
        numMsgRead++;
    }
    memset(node->socket.buffer, 0, MAX_BUFFER_SIZE);
    if(token != NULL)
        strcpy(node->socket.buffer, token);
}

void queueCommunication(AppNode *app, NodeQueue *queue, fd_set *currentSockets, int pos)
{
    char buffer_cpy[MAX_BUFFER_SIZE] = "\0", *token, cmd[24];
    int numCompletedMsg , numMsgRead = 0, promotedFlag = 0;
    numCompletedMsg = countLFchars(queue->queue[pos].socket.buffer);
    if(numCompletedMsg == 0)
        return;

    strcpy(buffer_cpy, queue->queue[pos].socket.buffer);
    token = strtok(buffer_cpy, "\n");

    while (numMsgRead < numCompletedMsg)
    {
        strcpy(cmd, token);
        cmd[strcspn(cmd, " ")] = '\0';
        promotedFlag = handleNEWmessage(app, queue, currentSockets, pos, cmd, token);
        token = advancePointer(token);
        token = strtok(token, "\n");
        numMsgRead++;
    }
    if(promotedFlag == 1)
    {
        memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);
        if(token != NULL)
            strcpy(app->ext.socket.buffer, token);
    }
    else if(promotedFlag == 0)
    { 
        memset(app->interns.intr[app->interns.numIntr - 1].socket.buffer, 0, MAX_BUFFER_SIZE);
        if(token != NULL)
            strcpy(app->interns.intr[app->interns.numIntr - 1].socket.buffer, token);
    }
    else
    {
        memset(queue->queue[pos].socket.buffer, 0, MAX_BUFFER_SIZE);
        if(token != NULL)
            strcpy(queue->queue[pos].socket.buffer, token);
    }
}

void closedExtConnection(AppNode *app, fd_set *currentSockets)
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

    writeMessageToInterns(app, buffer);

    if (strcmp(app->self.id, app->bck.id) == 0) // sou ancora
    {
        if (app->interns.numIntr > 0) // escolhe um dos internos para ser o seu novo externo
        {
            // promove interno a externo e retira-o da lista de internos
            FD_CLR(app->ext.socket.fd, currentSockets);
            promoteInternToExtern(app);
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
        connectToBackup(app, currentSockets);
    }
}

void closedIntConnection(AppNode *app, fd_set *currentSockets, int i)
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
    }
    memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);

    memmove(&app->interns.intr[i], &app->interns.intr[app->interns.numIntr - 1], sizeof(NODE));
    memset(&app->interns.intr[app->interns.numIntr - 1], 0, sizeof(NODE));
    app->interns.numIntr--;
    writeMessageToInterns(app, buffer);
}

int readTcp(SOCKET *socket)
{
    ssize_t n;
    char *buffer_cpy = (char *) calloc(MAX_BUFFER_SIZE, sizeof(char));
    if(buffer_cpy == NULL)
    {
        printf("Error in calloc\n");
        return 0;
    }

    if((n = read((*socket).fd, buffer_cpy, MAX_BUFFER_SIZE)) <= 0)
    {
        close((*socket).fd);
        free(buffer_cpy);
        return -1;
    }
    else
    {
        strcat(socket->buffer, buffer_cpy);
    }
    free(buffer_cpy);
    return 0;
}

int writeTcp(SOCKET socket)
{
    ssize_t n_ToSend, n_Sent, n_totalSent = 0;
    n_ToSend = strlen(socket.buffer);
    while (n_totalSent < n_ToSend)
    {
        n_Sent = write(socket.fd, socket.buffer, strlen(socket.buffer));
        if (n_Sent <= 0)
        {
            close(socket.fd);
            return -1;
        }
        n_totalSent += n_Sent;
    }

    return 0;
}

void writeMessageToInterns(AppNode *app, char *buffer)
{
    for (int i = 0; i < app->interns.numIntr; i++)
    {
        strcpy(app->interns.intr[i].socket.buffer, buffer);
        if (writeTcp(app->interns.intr[i].socket) < 0)
        {
            printf("Can't write to intern\n");
        }
        memset(app->interns.intr[i].socket.buffer, 0, MAX_BUFFER_SIZE);
    }
}

void promoteInternToExtern(AppNode *app)
{
    memmove(&app->ext, &app->interns.intr[0], sizeof(NODE));
    memmove(&app->interns.intr[0], &app->interns.intr[app->interns.numIntr - 1], sizeof(NODE));
    memset(&app->interns.intr[app->interns.numIntr - 1], 0, sizeof(NODE));
    app->interns.numIntr--;


    sprintf(app->ext.socket.buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
    if (writeTcp(app->ext.socket) < 0)
    {
        printf("Can't write when I'm trying to connect\n");
        exit(1);
    }
    memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);

    writeMessageToInterns(app, app->ext.socket.buffer);
}

void connectToBackup(AppNode *app, fd_set *currentSockets)
{
    char buffer[64] = "\0";
    // altera o novo extern para o backup
    strcpy(app->ext.id, app->bck.id);
    strcpy(app->ext.ip, app->bck.ip);
    strcpy(app->ext.port, app->bck.port);

    if ((app->ext.socket.fd = connectTcpClient(app)) > 0) // join network by connecting to another node
    {
        FD_SET(app->ext.socket.fd, currentSockets); // sucessfully connected to ext node
        sprintf(app->ext.socket.buffer, "NEW %s %s %s\n", app->self.id, app->self.ip, app->self.port);
        if (writeTcp(app->ext.socket) < 0)
        {
            printf("Can't write when I'm trying to connect\n");
        }
        memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);

        if (app->interns.numIntr > 0) // tem nos intr e comunica com todos para atualizar os seus nos de backup
        {
            memset(buffer, 0, 64);
            sprintf(buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
            writeMessageToInterns(app, buffer);
        }
        updateExpeditionTable(app, app->ext.id, app->ext.id, app->ext.socket.fd);
    }
    else
    {
        printf("TCP ERROR!!\n");
        memmove(&app->ext, &app->self, sizeof(NODE)); // couldnt connect to ext node
    }
}
