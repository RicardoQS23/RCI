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
    if ((fd = accept(app->self.fd, (struct sockaddr *)&addr, &addrlen)) == -1)
        return -1;

    return fd;
}

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
        handleCONTENTmessage(app, app->ext, cmd, token);
        handleWITHDRAWmessage(app, app->ext, cmd, token);
        token = advancePointer(token);
        token = strtok(token, "\n");
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
        handleCONTENTmessage(app, node, cmd, token);
        handleWITHDRAWmessage(app, node, cmd, token);
        token = advancePointer(token);
        token = strtok(token, "\n");
    }
}

void closedExtConnection(AppNode *app, char *buffer, fd_set *currentSockets)
{
    updateExpeditionTable(app, app->ext.id, "-1", 0);
    for (int i = 0; i < 100; i++)
    {
        if (strcmp(app->expeditionTable[i].id, app->ext.id) == 0)
            updateExpeditionTable(app, app->expeditionTable[i].id, "-1", 0);
    }
    memset(buffer, 0, MAX_BUFFER_SIZE);
    sprintf(buffer, "WITHDRAW %s\n", app->ext.id);
    writeMessageToInterns(app, buffer);

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

void closedIntConnection(AppNode *app, fd_set *currentSockets, int i)
{
    char buffer[MAX_BUFFER_SIZE] = "\0";
    FD_CLR(app->interns.intr[i].fd, currentSockets);
    close(app->interns.intr[i].fd);

    updateExpeditionTable(app, app->interns.intr[i].id, "-1", 0);
    for (int i = 0; i < 100; i++)
    {
        if (strcmp(app->expeditionTable[i].id, app->interns.intr[i].id) == 0)
            updateExpeditionTable(app, app->expeditionTable[i].id, "-1", 0);
    }

    sprintf(buffer, "WITHDRAW %s\n", app->interns.intr[i].id);
    if (writeTcp(app->ext.fd, buffer) < 0)
    {
        printf("Can't send WITHDRAW msg to extern\n");
    }

    memmove(&app->interns.intr[i], &app->interns.intr[app->interns.numIntr - 1], sizeof(NODE));
    memset(&app->interns.intr[app->interns.numIntr - 1], 0, sizeof(NODE));
    app->interns.numIntr--;
    writeMessageToInterns(app, buffer);
}

int readTcp(int fd, char *buffer)
{
    char *buffer_cpy = (char *) malloc(MAX_BUFFER_SIZE * sizeof(char));
    ssize_t n;
    memset(buffer, 0, MAX_BUFFER_SIZE);
    while ((n = read(fd, buffer, MAX_BUFFER_SIZE)) < 0)
    {
        strcat(buffer, buffer_cpy);
    }

    if ((n == 0 && strlen(buffer) == 0) || n == -1)
    {
        close(fd);
        free(buffer_cpy);
        return -1;
    }
    free(buffer_cpy);
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
        //printf("sent: %s\n", buffer);
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
