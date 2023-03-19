#include "project.h"

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

void handleCONTENTmessage(AppNode *app, NODE node, char *cmd, char *token)
{
    char dest_id[3], orig_id[3], name[100];
    char buffer[MAX_BUFFER_SIZE] = "\0";

    if (strcmp(cmd, "CONTENT") == 0)
    {
        if (sscanf(token, "CONTENT %s %s %s", dest_id, orig_id, name) != 3)
        {
            printf("Cant read CONTENT msg\n");
        }
        updateExpeditionTable(app, orig_id, node.id, node.fd);
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
        updateExpeditionTable(app, orig_id, node.id, node.fd);
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

void handleWITHDRAWmessage(AppNode *app, NODE node, char *cmd, char *token)
{
    char withdrawn_id[3];

    if (strcmp(cmd, "WITHDRAW") == 0)
    {
        if (sscanf(token, "WITHDRAW %s", withdrawn_id) != 1)
        {
            printf("Cant read WITHDRAW msg\n");
        }
        updateExpeditionTable(app, withdrawn_id, "-1", 0);
        for (int i = 0; i < 100; i++)
        {
            if (strcmp(app->expeditionTable[i].id, withdrawn_id) == 0)
                updateExpeditionTable(app, app->expeditionTable[i].id, "-1", 0);
        }
        shareWITHDRAWmessages(app, node, token);
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

void shareWITHDRAWmessages(AppNode *app, NODE node, char *buffer)
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