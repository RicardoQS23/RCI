#include "project.h"

int handleNEWmessage(AppNode *app, NodeQueue *queue, fd_set *currentSockets, int pos, char *cmd, char *token)
{
    if (strcmp(cmd, "NEW") == 0)
    {
        if (sscanf(token, "NEW %s %s %s", queue->queue[pos].id, queue->queue[pos].ip, queue->queue[pos].port) != 3)
        {
            printf("Bad response from queue\n");
            return -1;
        }
        promoteQueueToIntern(app, queue, currentSockets, pos);
        if (strcmp(app->ext.id, app->self.id) == 0)
        {
            promoteInternToExtern(app);
            return 1;
        }
        else
        {
            sprintf(app->interns.intr[app->interns.numIntr - 1].socket.buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
            if (writeTcp(app->interns.intr[app->interns.numIntr - 1].socket) < 0)
            {
                printf("Can't write when someone is trying to connect\n");
            }
            memset(app->interns.intr[app->interns.numIntr - 1].socket.buffer, 0, MAX_BUFFER_SIZE);

            return 0;
        }
    }
    return -1;
}

void handleFirstEXTmessage(AppNode *app, NODE *temporaryExtern, char *cmd, char *token)
{
    if (strcmp(cmd, "EXTERN") == 0)
    {
        if (sscanf(token, "EXTERN %s %s %s", app->bck.id, app->bck.ip, app->bck.port) != 3)
        {
            printf("Bad response from server\n");
        }
        promoteTemporaryToExtern(app, temporaryExtern);
        if (strcmp(app->bck.id, app->self.id) == 0)
            memmove(&app->bck, &app->self, sizeof(NODE));
        else
            updateExpeditionTable(app, app->bck.id, app->ext.id, app->ext.socket.fd);
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
            updateExpeditionTable(app, app->bck.id, app->ext.id, app->ext.socket.fd);
    }
}

void handleQUERYmessage(AppNode *app, NODE node, char *cmd, char *token)
{
    if (strcmp(cmd, "QUERY") == 0)
    {
        char dest_id[3], orig_id[3], name[100];
        char buffer[MAX_BUFFER_SIZE] = "\0";
        SOCKET expTableSocket;

        if (sscanf(token, "QUERY %s %s %s", dest_id, orig_id, name) != 3)
        {
            printf("Bad response from server\n");
        }
        updateExpeditionTable(app, orig_id, node.id, node.socket.fd);
        if (strcmp(app->self.id, dest_id) == 0) // mensagem de query chegou ao destino
        {
            expTableSocket.fd = app->expeditionTable[atoi(orig_id)].fd;
            if (searchContentOnList(app, name) > 0)
                sprintf(expTableSocket.buffer, "CONTENT %s %s %s\n", orig_id, dest_id, name);
            else
                sprintf(expTableSocket.buffer, "NOCONTENT %s %s %s\n", orig_id, dest_id, name);

            if (writeTcp(expTableSocket) < 0)
            {
                printf("Cant send Content message\n");
                return;
            }
        }
        else // propagar queries pelos vizinhos
        {
            sprintf(buffer, "QUERY %s %s %s\n", dest_id, orig_id, name);
            shareQUERYmessages(app, node, buffer, dest_id);
        }
    }
}

void handleCONTENTmessage(AppNode *app, NODE node, char *cmd, char *token)
{
    char dest_id[3], orig_id[3], name[100];
    SOCKET expTableSocket;

    if (strcmp(cmd, "CONTENT") == 0)
    {
        if (sscanf(token, "CONTENT %s %s %s", dest_id, orig_id, name) != 3)
        {
            printf("Cant read CONTENT msg\n");
        }
        updateExpeditionTable(app, orig_id, node.id, node.socket.fd);
        if (strcmp(app->self.id, dest_id) == 0) // mensagem de content chegou ao destino
        {
            printf("%s is present on %s\n", name, orig_id);
        }
        else // enviar msg de content pelo vizinho que possui a socket para comunicar com o nó destino
        {
            expTableSocket.fd = app->expeditionTable[atoi(dest_id)].fd;
            sprintf(expTableSocket.buffer, "CONTENT %s %s %s\n", dest_id, orig_id, name);

            if (writeTcp(expTableSocket) < 0)
            {
                printf("Cant send Content message\n");
                return;
            }
        }
    }
    else if (strcmp(cmd, "NOCONTENT") == 0)
    {
        if (sscanf(token, "NOCONTENT %s %s %s", dest_id, orig_id, name) != 3)
        {
            printf("Cant read NOCONTENT msg\n");
        }
        updateExpeditionTable(app, orig_id, node.id, node.socket.fd);
        if (strcmp(app->self.id, dest_id) == 0) // mensagem de content chegou ao destino
        {
            printf("%s is not present on %s\n", name, orig_id);
        }
        else // enviar msg de no_content pelo vizinho que possui a socket para comunicar com o nó destino
        {
            expTableSocket.fd = app->expeditionTable[atoi(dest_id)].fd;
            sprintf(expTableSocket.buffer, "NOCONTENT %s %s %s\n", dest_id, orig_id, name);

            if (writeTcp(expTableSocket) < 0)
            {
                printf("Cant send NoContent message\n");
                return;
            }
        }
    }
}

void handleWITHDRAWmessage(AppNode *app, NODE node, char *cmd, char *token)
{
    if (strcmp(cmd, "WITHDRAW") == 0)
    {
        char withdrawn_id[3];
        char buffer[64] = "\0";
        if (sscanf(token, "WITHDRAW %s", withdrawn_id) != 1)
        {
            printf("Cant read WITHDRAW msg\n");
            return;
        }
        updateExpeditionTable(app, withdrawn_id, "-1", 0);
        for (int i = 0; i < 100; i++)
        {
            if (strcmp(app->expeditionTable[i].id, withdrawn_id) == 0)
                updateExpeditionTable(app, app->expeditionTable[i].id, "-1", 0);
        }
        strcpy(buffer, token);
        strcat(buffer, "\n");
        shareWITHDRAWmessages(app, node, buffer);
    }
}

void shareQUERYmessages(AppNode *app, NODE node, char *buffer, char *dest_id)
{
    SOCKET expTableSocket;
    if (app->expeditionTable[atoi(dest_id)].fd == 0) // ainda n possui o no destino da tabela de expediçao
    {
        if (strcmp(node.id, app->ext.id) != 0)
        {
            strcpy(app->ext.socket.buffer, buffer);
            if (writeTcp(app->ext.socket) < 0)
            {
                printf("Can't write when sending queries\n");
                return;
            }
            memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);
        }
        for (int i = 0; i < app->interns.numIntr; i++)
        {
            if (strcmp(node.id, app->interns.intr[i].id) != 0)
            {
                strcpy(app->interns.intr[i].socket.buffer, buffer);
                if (writeTcp(app->interns.intr[i].socket) < 0)
                {
                    printf("Can't write when sending queries\n");
                    return;
                }
                memset(app->interns.intr[i].socket.buffer, 0, MAX_BUFFER_SIZE);

            }
        }
    }
    else
    {
        expTableSocket.fd = app->expeditionTable[atoi(dest_id)].fd;
        strcpy(expTableSocket.buffer, buffer);
        if (writeTcp(expTableSocket) < 0)
        {
            printf("Can't write when someone is trying to connect\n");
            return;
        }
    }
}

void shareWITHDRAWmessages(AppNode *app, NODE node, char *token)
{
    if (strcmp(node.id, app->ext.id) != 0)
    {
        strcpy(app->ext.socket.buffer, token);
        if (writeTcp(app->ext.socket) < 0)
        {
            printf("Can't write when sending queries\n");
            return;
        }
        memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);
    }
    for (int i = 0; i < app->interns.numIntr; i++)
    {
        if (strcmp(node.id, app->interns.intr[i].id) != 0)
        {
            strcpy(app->interns.intr[i].socket.buffer, token);
            if (writeTcp(app->interns.intr[i].socket) < 0)
            {
                printf("Can't write when sending queries\n");
                return;
            }
            memset(app->interns.intr[i].socket.buffer, 0, MAX_BUFFER_SIZE);
        }
    }
}