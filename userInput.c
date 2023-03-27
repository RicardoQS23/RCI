#include "project.h"

/**
 * @brief sadhjasdhkjasdjkakdaskj
 */
void joinCommand(AppNode *app, NODE *temporaryExtern, fd_set *currentSockets, char *regIP, char *regUDP, char *net)
{
    char buffer[MAX_BUFFER_SIZE] = "\0", serverResponse[16] = "\0";

    memmove(&app->ext, &app->self, sizeof(NODE));
    memmove(&app->bck, &app->self, sizeof(NODE));

    sprintf(buffer, "NODES %s", net);
    sprintf(serverResponse, "NODESLIST %s\n", net);
    udpClient(buffer, regIP, regUDP, net); // gets net nodes
    if (chooseRandomNodeToConnect(buffer, app->self.id) == 1)
    {
        printf("Node's already on the net\n");
        return;
    }
    else
    {
        if (strcmp(buffer, serverResponse) == 0) // rede esta vazia
        {
            regNetwork(app, currentSockets, regIP, regUDP, net);
        }
        else
        {
            if (sscanf(buffer, "%s %s %s", temporaryExtern->id, temporaryExtern->ip, temporaryExtern->port) == 3)
            {
                if ((temporaryExtern->socket.fd = connectTcpClient(app)) > 0) // join network by connecting to another node
                {
                    memset(temporaryExtern->socket.buffer, 0, MAX_BUFFER_SIZE);
                    sprintf(temporaryExtern->socket.buffer, "NEW %s %s %s\n", app->self.id, app->self.ip, app->self.port);
                    if (writeTcp(temporaryExtern->socket) < 0)
                    {
                        printf("Can't write when I'm trying to connect\n");
                        memmove(&(*temporaryExtern), &app->self, sizeof(NODE));
                        return;
                    }
                    memset(temporaryExtern->socket.buffer, 0, MAX_BUFFER_SIZE);
                    // adicionar externo aos vizinhos
                    regNetwork(app, currentSockets, regIP, regUDP, net);
                    FD_SET(temporaryExtern->socket.fd, currentSockets);
                }
                else
                {
                    printf("Couldn't connect\n");
                    memmove(&(*temporaryExtern), &app->self, sizeof(NODE));
                }
            }
        }
    }
}

void djoinCommand(AppNode *app, fd_set *currentSockets, char *bootID, char *bootIP, char *bootTCP)
{
    memmove(&app->ext, &app->self, sizeof(NODE));
    memmove(&app->bck, &app->self, sizeof(NODE));
    strcpy(app->ext.id, bootID);
    strcpy(app->ext.ip, bootIP);
    strcpy(app->ext.port, bootTCP);

    if ((app->ext.socket.fd = connectTcpClient(app)) > 0) // join network by connecting to another node
    {
        memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);
        sprintf(app->ext.socket.buffer, "NEW %s %s %s\n", app->self.id, app->self.ip, app->self.port);
        if (writeTcp(app->ext.socket) < 0)
        {
            printf("Can't write when I'm trying to connect\n");
            memmove(&app->ext, &app->self, sizeof(NODE));
            return;
        }
        memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);
        FD_SET(app->ext.socket.fd, currentSockets);
    }
    else
    {
        printf("TCP ERROR!!\n");
        memmove(&app->ext, &app->self, sizeof(NODE)); // couldnt connect to ext node
    }
}

void leaveCommand(AppNode *app, fd_set *currentSockets, char *regIP, char *regUDP, char *net)
{
    clearExpeditionTable(app);
    unregNetwork(app, currentSockets, regIP, regUDP, net);
    // Closing sockets
    if (strcmp(app->ext.id, app->self.id) != 0) // has extern
    {
        FD_CLR(app->ext.socket.fd, currentSockets);
        close(app->ext.socket.fd);
    }
    if (app->interns.numIntr > 0)
    {
        for (int i = 0; i < app->interns.numIntr; i++)
        {
            FD_CLR(app->interns.intr[i].socket.fd, currentSockets);
            close(app->interns.intr[i].socket.fd);
        }
        app->interns.numIntr = 0;
    }
    memmove(&app->ext, &app->self, sizeof(NODE));
    memmove(&app->bck, &app->self, sizeof(NODE));
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

        if (app->contentList == NULL)
        {
            app->contentList = new_node;
            return;
        }

        LinkedList *list = app->contentList; // use a separate pointer to iterate over the list

        while (list->next != NULL)
        {
            list = list->next; // advance to the next node in the list
        }
        list->next = new_node; // add the new node to the end of the list
    }
    else
        printf("Name's already on Content List\n");
}

void loadCommand(AppNode *app, char *fileName)
{
    FILE *fp = NULL;
    char buffer[MAX_BUFFER_SIZE], *token, name[100];

    if ((fp = fopen(fileName, "r")) == NULL)
    {
        printf("Can't open %s file\n", fileName);
        return;
    }
    while (fgets(buffer, MAX_BUFFER_SIZE, fp) != NULL)
    {
        token = strtok(buffer, " \n\t\r");
        while (token != NULL)
        {
            strcpy(name, token);
            createCommand(app, name);
            token = strtok(NULL, " \n\t\r");
        }
    }
    fclose(fp);
}

void clearNamesCommand(AppNode *app)
{
    freeContentList(app);
}

void clearRoutingCommand(AppNode *app)
{
    char id[3];
    for (int i = 0; i < 100; i++)
    {
        sprintf(id, "%02d", i);
        updateExpeditionTable(app, id, "-1", 0);
    }
}

void deleteCommand(AppNode *app, char *name)
{
    LinkedList *ptr, *aux, *head;
    head = app->contentList;
    ptr = app->contentList;

    for (aux = ptr; aux != NULL; aux = aux->next)
    {
        if (strcmp(aux->contentName, name) == 0)
        {
            if (aux == head) // Head of the LinkedList
            {
                app->contentList = aux->next;
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

void getCommand(AppNode *app, char *dest, char *name)
{
    char buffer[MAX_BUFFER_SIZE] = "\0";
    SOCKET expTableSocket;
    sprintf(buffer, "QUERY %s %s %s\n", dest, app->self.id, name);

    if (app->expeditionTable[atoi(dest)].fd == 0) // ainda n possui a melhor rota para chegar ao destino
    {
        strcpy(app->ext.socket.buffer, buffer);
        if (writeTcp(app->ext.socket) < 0)
        {
            printf("Cant send QUERY msg to extern\n");
        }
        memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);
        writeMessageToInterns(app, buffer);
    }
    else
    {
        expTableSocket.fd = app->expeditionTable[atoi(dest)].fd;
        strcpy(expTableSocket.buffer, buffer);
        if (writeTcp(expTableSocket) < 0)
        {
            printf("Cant send QUERY msg\n");
        }
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

void showNamesCommand(AppNode *app)
{
    LinkedList *aux;
    printf("---- CONTENT LIST ----\n");
    for (aux = app->contentList; aux != NULL; aux = aux->next)
    {
        printf("%s\n", aux->contentName);
    }
}

void showRoutingCommand(AppNode *app)
{
    char id[3];
    printf("---- Expedition Table ----\n");
    for (int i = 0; i < 100; i++)
    {
        if (app->expeditionTable[i].fd > 0)
        {
            sprintf(id, "%02d", i);
            printf("%s -> %s\n", id, app->expeditionTable[i].id);
        }
    }
}

void commandMultiplexer(AppNode *app, NODE *temporaryExtern, enum commands cmd, fd_set *currentSockets, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName)
{
    switch (cmd)
    {
    case JOIN:
        // Nodes's initialization
        joinCommand(app, temporaryExtern, currentSockets, regIP, regUDP, net);
        break;
    case DJOIN:
        // Nodes's initialization
        djoinCommand(app, currentSockets, bootID, bootIP, bootTCP);
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
        leaveCommand(app, currentSockets, regIP, regUDP, net);
        break;
    case CLEAR_NAMES:
        clearNamesCommand(app);
        break;
    case CLEAR_ROUTING:
        clearRoutingCommand(app);
        break;
    case LOAD:
        loadCommand(app, fileName);
        break;
    case EXIT:
        close(app->self.socket.fd);
        freeContentList(app);
        exit(0);
    default:
        printf("unknown command\n");
        break;
    }
}
