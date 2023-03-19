#include "project.h"

void updateExpeditionTable(AppNode *app, char *dest_id, char *neigh_id, int neigh_fd)
{
    strcpy(app->expeditionTable[atoi(dest_id)].id, neigh_id);
    app->expeditionTable[atoi(dest_id)].fd = neigh_fd;
}

void clearExpeditionTable(AppNode *app)
{
    char id[3];
    for (int i = 0; i < 100; i++)
    {
        sprintf(id, "%02d", i);
        updateExpeditionTable(app, id, "-1", 0);
    }
}

int searchContentOnList(AppNode *app, char *name)
{
    LinkedList *aux;
    aux = app->self.contentList;
    while (aux != NULL)
    {
        if (strcmp(aux->contentName, name) == 0)
            return 1;
        aux = aux->next;
    }
    return 0;
}

void freeContentList(AppNode *app)
{
    LinkedList *aux, *ptr;
    aux = ptr = app->self.contentList;
    while (aux != NULL)
    {
        aux = aux->next;
        free(ptr);
    }
}