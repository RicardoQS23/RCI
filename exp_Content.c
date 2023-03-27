#include "project.h"

/**
 * @brief In this function, the expedition table is updated  
 */
void updateExpeditionTable(AppNode *app, char *dest_id, char *neigh_id, int neigh_fd)
{
    strcpy(app->expeditionTable[atoi(dest_id)].id, neigh_id);
    app->expeditionTable[atoi(dest_id)].fd = neigh_fd;
}

/**
 * @brief This function table cleans the expedition when the CLEAR ROUTING or the LEAVE comand is issued 
 */
void clearExpeditionTable(AppNode *app)
{
    char id[3];
    for (int i = 0; i < 100; i++)
    {
        sprintf(id, "%02d", i);
        updateExpeditionTable(app, id, "-1", 0);
    }
}

/**
 * @brief This function searches for the content "name" in the content list of a node
 */
int searchContentOnList(AppNode *app, char *name)
{
    LinkedList *aux;
    aux = app->contentList;
    while (aux != NULL)
    {
        if (strcmp(aux->contentName, name) == 0)
            return 1;
        aux = aux->next;
    }
    return 0;
}

/**
 * @brief This function clears the content list of a node 
 */
void freeContentList(AppNode *app)
{
    LinkedList *ptr, *aux;
    aux = app->contentList;
    while(aux != NULL)
    {
        ptr = aux;
        app->contentList = aux->next;
        aux = aux->next;
        free(ptr);
    }
}