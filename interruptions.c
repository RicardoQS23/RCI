#include "project.h"

/**
 * @brief This function is responsible for the handling of all interruptions. These interruptions can be originated from
 * several sources, those being user input, server comunication, extern, interns, temporary extern and from the node queue. This function
 * calls several other functions, one for each of the possible sources mentioned.
 */
void handleInterruptions(AppNode *app, NodeQueue *queue, NODE *temporaryExtern, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName)
{
    handleUserInputInterruption(app, temporaryExtern, readSockets, currentSockets, cmd, bootIP, name, dest, bootID, bootTCP, net, regIP, regUDP, fileName);
    handleServerInterruption(app, queue, readSockets, currentSockets);
    handleExtInterruption(app, temporaryExtern, readSockets, currentSockets);
    handleInternInterruptions(app, readSockets, currentSockets);
    handleQueueInterruptions(app, queue, readSockets, currentSockets);
    handleTemporaryExternInterruption(app, temporaryExtern, readSockets, currentSockets);
}

/**
 * @brief This function handles user-related interruptions
 */
void handleUserInputInterruption(AppNode *app, NODE *temporaryExtern, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName)
{
    if (FD_ISSET(0, readSockets))
    {
        char buffer[MAX_BUFFER_SIZE] = "\0";
        FD_CLR(0, readSockets);
        if (fgets(buffer, MAX_BUFFER_SIZE, stdin) == NULL)
        {
            printf("Couldn't read user input\n");
            return;
        }
        if (validateUserInput(app, cmd, buffer, bootIP, name, dest, bootID, bootTCP, net, fileName) < 0)
        {
            printf("bad user input\n");
            return;
        }
        commandMultiplexer(app, temporaryExtern, *cmd, currentSockets, buffer, bootIP, name, dest, bootID, bootTCP, net, regIP, regUDP, fileName);
    }
}

/**
 * @brief This function handles server-related interruptions
 */
void handleServerInterruption(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets)
{
    if (FD_ISSET(app->self.socket.fd, readSockets))
    {
        FD_CLR(app->self.socket.fd, readSockets);
        acceptNeighbourConnection(app, queue, currentSockets);
    }
}

/**
 * @brief This function handles extern-related interruptions
 */
void handleExtInterruption(AppNode *app, NODE *temporaryExtern, fd_set *readSockets, fd_set *currentSockets)
{
    if (FD_ISSET(app->ext.socket.fd, readSockets))
    {
        FD_CLR(app->ext.socket.fd, readSockets);
        if (readTcp(&(app->ext.socket)) < 0)
        {
            closedExtConnection(app, temporaryExtern, currentSockets);
        }
        else
        {
            handleCommunication(app, &(app->ext));
        }
    }
}

/**
 * @brief This function handles intern-related interruptions
 */
void handleInternInterruptions(AppNode *app, fd_set *readSockets, fd_set *currentSockets)
{
    for (int i = app->interns.numIntr - 1; i >= 0; i--)
    {
        if (FD_ISSET(app->interns.intr[i].socket.fd, readSockets))
        {
            FD_CLR(app->interns.intr[i].socket.fd, readSockets);
            if (readTcp(&(app->interns.intr[i].socket)) < 0)
            {
                closedIntConnection(app, currentSockets, i);
            }
            else
            {
                handleCommunication(app, &(app->interns.intr[i]));
            }
        }
    }
}

/**
 * @brief This function handles queue-related interruptions
 */
void handleQueueInterruptions(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets)
{
    for (int i = queue->numNodesInQueue - 1; i >= 0; i--)
    {
        if (FD_ISSET(queue->queue[i].socket.fd, readSockets))
        {
            FD_CLR(queue->queue[i].socket.fd, readSockets);
            if (readTcp(&(queue->queue[i].socket)) < 0)
            {
                FD_CLR(queue->queue[i].socket.fd, currentSockets);
                close(queue->queue[i].socket.fd);
                popQueue(queue, currentSockets, i);
            }
            else
            {
                queueCommunication(app, queue, currentSockets, i);
            }
        }
    }
}

/**
 * @brief This function handles temporary extern-related interruptions
 */
void handleTemporaryExternInterruption(AppNode *app, NODE *temporaryNode, fd_set *readSockets, fd_set *currentSockets)
{
    if (FD_ISSET(temporaryNode->socket.fd, readSockets))
    {
        FD_CLR(temporaryNode->socket.fd, readSockets);
        if (readTcp(&(temporaryNode->socket)) < 0)
        {
            resetTemporaryExtern(temporaryNode, currentSockets);
            memmove(&app->bck, &app->self, sizeof(NODE));
            if (app->interns.numIntr > 0) // escolhe um dos internos para ser o seu novo externo
            {
                // promove interno a externo e retira-o da lista de internos
                promoteInternToExtern(app);
            }
            else // ficou sozinho na rede
            {
                memmove(&app->ext, &app->self, sizeof(NODE));
            }
        }
        else
        {
            temporaryExternCommunication(app, temporaryNode, currentSockets);
        }
    }
}