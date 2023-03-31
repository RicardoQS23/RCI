#include "project.h"

/**
 * @brief Handles 'NEW' messages
 */
int handleNEWmessage(AppNode *app, NodeQueue *queue, NODE *temporaryExtern, fd_set *currentSockets, int pos, char *token)
{
    if (sscanf(token, "NEW %s %s %s", queue->queue[pos].id, queue->queue[pos].ip, queue->queue[pos].port) != 3)
    {
        printf("Bad response from queue\n");
        return -1;
    }
    promoteQueueToIntern(app, queue, pos);
    if (strcmp(app->ext.id, app->self.id) == 0)
    {
        promoteInternToExtern(app, temporaryExtern, currentSockets);
        return 1;
    }
    else
    {
        sprintf(app->interns.intr[app->interns.numIntr - 1].socket.buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
        if (writeTcp(app->interns.intr[app->interns.numIntr - 1].socket) < 0)
        {
            printf("Can't write Extern message\n");
        }
        memset(app->interns.intr[app->interns.numIntr - 1].socket.buffer, 0, MAX_BUFFER_SIZE);

        return 0;
    }
    return -1;
}

/**
 * @brief Handles the 'EXTERN' message coming from the temporary extern node
 */
int handleFirstEXTmessage(AppNode *app, NODE *temporaryExtern, char *token)
{
    int promotedFlag = 0;
    if (sscanf(token, "EXTERN %s %s %s", app->bck.id, app->bck.ip, app->bck.port) != 3)
    {
        printf("Bad response from server\n");
        memmove(&app->bck, &app->self, sizeof(NODE));
        return 0;
    }
    promoteTemporaryToExtern(app, temporaryExtern);
    promotedFlag = 1;
    if (strcmp(app->bck.id, app->self.id) == 0)
        memmove(&app->bck, &app->self, sizeof(NODE));
    /*else
        updateExpeditionTable(app, app->bck.id, app->ext.id, app->ext.socket.fd);*/
    return promotedFlag;
}

/**
 * @brief Handles 'EXTERN' messages
 */
void handleEXTmessage(AppNode *app, char *token)
{
    if (sscanf(token, "EXTERN %s %s %s", app->bck.id, app->bck.ip, app->bck.port) != 3)
    {
        printf("Bad response from server\n");
        memmove(&app->bck, &app->self, sizeof(NODE));
        return;
    }
    if (strcmp(app->bck.id, app->self.id) == 0)
        memmove(&app->bck, &app->self, sizeof(NODE));
    /*else
        updateExpeditionTable(app, app->bck.id, app->ext.id, app->ext.socket.fd);*/
}

/**
 * @brief Handles 'QUERY' messages
 */
void handleQUERYmessage(AppNode *app, NODE node, char *token)
{
    char dest_id[3] = "\0", orig_id[3] = "\0", name[100] = "\0";
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
            printf("Cant write Content message\n");
            return;
        }
    }
    else // Propagates the querie message to the neighbor nodes
    {
        sprintf(buffer, "QUERY %s %s %s\n", dest_id, orig_id, name);
        shareQUERYmessages(app, node, buffer, dest_id);
    }
}

/**
 * @brief Handle 'NOCONTENT' messages
 */
void handleNOCONTENTmessage(AppNode *app, NODE node, char *token)
{
    char dest_id[3] = "\0", orig_id[3] = "\0", name[100] = "\0";
    SOCKET expTableSocket;

    if (sscanf(token, "NOCONTENT %s %s %s", dest_id, orig_id, name) != 3)
    {
        printf("Cant read NOCONTENT msg\n");
    }
    updateExpeditionTable(app, orig_id, node.id, node.socket.fd);
    if (strcmp(app->self.id, dest_id) == 0) // mensagem de content chegou ao destino
    {
        printf("\033[31m%s is not present on %s\033[0m\n", name, orig_id);
    }
    else // enviar msg de no_content pelo vizinho que possui a socket para comunicar com o nó destino
    {
        expTableSocket.fd = app->expeditionTable[atoi(dest_id)].fd;
        sprintf(expTableSocket.buffer, "NOCONTENT %s %s %s\n", dest_id, orig_id, name);

        if (writeTcp(expTableSocket) < 0)
        {
            printf("Cant write NoContent message\n");
            return;
        }
    }
}

/**
 * @brief Handles 'CONTENT' messages
 */
void handleCONTENTmessage(AppNode *app, NODE node, char *token)
{
    char dest_id[3] = "\0", orig_id[3] = "\0", name[100] = "\0";
    SOCKET expTableSocket;

    if (sscanf(token, "CONTENT %s %s %s", dest_id, orig_id, name) != 3)
    {
        printf("Cant read CONTENT msg\n");
    }
    updateExpeditionTable(app, orig_id, node.id, node.socket.fd);
    if (strcmp(app->self.id, dest_id) == 0) // mensagem de content chegou ao destino
    {
        printf("\033[32m%s is present on %s\033[0m\n", name, orig_id);
    }
    else // enviar msg de content pelo vizinho que possui a socket para comunicar com o nó destino
    {
        expTableSocket.fd = app->expeditionTable[atoi(dest_id)].fd;
        sprintf(expTableSocket.buffer, "CONTENT %s %s %s\n", dest_id, orig_id, name);

        if (writeTcp(expTableSocket) < 0)
        {
            printf("Cant write Content message\n");
            return;
        }
    }
}

/**
 * @brief Handles 'WITHDRAW' messages
 */
void handleWITHDRAWmessage(AppNode *app, NODE node, char *token)
{
    char withdrawn_id[3] = "\0";
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

/**
 * @brief This function shares the 'QUERY' message recieved with its neighbors
 */
void shareQUERYmessages(AppNode *app, NODE node, char *buffer, char *dest_id)
{
    SOCKET expTableSocket;
    if (app->expeditionTable[atoi(dest_id)].fd == 0) // ainda n possui o no destino da tabela de expediçao
    {
        if (strcmp(node.id, app->ext.id) != 0) // If it wasn't the extern who sent the query message, send the query to him aswell
        {
            strcpy(app->ext.socket.buffer, buffer);
            if (writeTcp(app->ext.socket) < 0)
            {
                printf("Can't write Query message\n");
                return;
            }
            memset(app->ext.socket.buffer, 0, MAX_BUFFER_SIZE);
        }
        for (int i = 0; i < app->interns.numIntr; i++) // Shares the query with its interns
        {
            if (strcmp(node.id, app->interns.intr[i].id) != 0)
            {
                strcpy(app->interns.intr[i].socket.buffer, buffer);
                if (writeTcp(app->interns.intr[i].socket) < 0)
                {
                    printf("Can't write Query message\n");
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
            printf("Can't write Query message\n");
            return;
        }
    }
}

/**
 * @brief This function shares the 'WITHDRAW' message recieved with its neighbors
 */
void shareWITHDRAWmessages(AppNode *app, NODE node, char *token)
{
    if (strcmp(node.id, app->ext.id) != 0)
    {
        strcpy(app->ext.socket.buffer, token);
        if (writeTcp(app->ext.socket) < 0)
        {
            printf("Can't write Withdraw message\n");
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
                printf("Can't write Withdraw message\n");
                return;
            }
            memset(app->interns.intr[i].socket.buffer, 0, MAX_BUFFER_SIZE);
        }
    }
}