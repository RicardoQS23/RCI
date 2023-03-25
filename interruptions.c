#include "project.h"

void handleInterruptions(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName)
{
    handleUserInputInterruption(app, readSockets, currentSockets, cmd, buffer, bootIP, name, dest, bootID, bootTCP, net, regIP, regUDP, fileName);
    handleServerInterruption(app, queue, readSockets, currentSockets);
    handleExtInterruption(app, buffer, readSockets, currentSockets);
    handleInternInterruptions(app, readSockets, currentSockets, buffer);
    handleQueueInterruptions(app, queue, readSockets, currentSockets, buffer);
}


void handleUserInputInterruption(AppNode *app, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *buffer, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName)
{
    if (FD_ISSET(0, readSockets))
    {
        FD_CLR(0, readSockets);
        if(fgets(buffer, MAX_BUFFER_SIZE, stdin) == NULL)
        {
            printf("Couldn't read user input\n");
            return;
        }
        if (validateUserInput(cmd, buffer, bootIP, name, dest, bootID, bootTCP, net, fileName, app) < 0)
        {
            printf("bad user input\n");
            return;
        }
        commandMultiplexer(app, buffer, *cmd, currentSockets, bootIP, name, dest, bootID, bootTCP, net, regIP, regUDP, fileName);
    }
}

void handleServerInterruption(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets)
{
    if (FD_ISSET(app->self.fd, readSockets))
    {
        printf("Someone is trying to connect!\n");
        FD_CLR(app->self.fd, readSockets);
        acceptNeighbourConnection(app, queue, currentSockets);
    }
}

void handleExtInterruption(AppNode *app, char *buffer, fd_set *readSockets, fd_set *currentSockets)
{
    if (FD_ISSET(app->ext.fd, readSockets)) // ext talks with us
    {
        FD_CLR(app->ext.fd, readSockets);
        if (readTcp(app->ext.fd, buffer) < 0) // conexao fechou e temos de estabelecer nova
        {
            printf("Extern's connection was closed\n");
            closedExtConnection(app, buffer, currentSockets);
        }
        else
        {
            printf("Extern is talking with us...\n");
            printf("recv: %s\n", buffer);
            externCommunication(app, buffer);
        }
    }
}

void handleInternInterruptions(AppNode *app, fd_set *readSockets, fd_set *currentSockets, char *buffer)
{
    for (int i = app->interns.numIntr - 1; i >= 0; i--)
    {
        if (FD_ISSET(app->interns.intr[i].fd, readSockets))
        {
            FD_CLR(app->interns.intr[i].fd, readSockets);
            if (readTcp(app->interns.intr[i].fd, buffer) < 0)
            {
                closedIntConnection(app, currentSockets, i);
            }
            else
            {
                printf("Intern is talking with us...\n");
                printf("recv: %s\n", buffer);
                internCommunication(app, app->interns.intr[i], buffer);
            }
        }
    }
}

void handleQueueInterruptions(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets, char *buffer)
{
    for (int i = queue->numNodesInQueue - 1; i >= 0; i--)
    {
        if (FD_ISSET(queue->queue[i].fd, readSockets))
        {
            FD_CLR(queue->queue[i].fd, readSockets);
            if (readTcp(queue->queue[i].fd, buffer) < 0)
            {
                FD_CLR(queue->queue[i].fd, currentSockets);
                close(queue->queue[i].fd);
                popQueue(queue, currentSockets, i);
            }
            else
            {
                printf("recv: %s\n", buffer);
                // validate msg read
                if (sscanf(buffer, "NEW %s %s %s\n", queue->queue[i].id, queue->queue[i].ip, queue->queue[i].port) != 3)
                {
                    printf("wrong format\n");
                    // TODO descartar nó malicioso da queue
                    exit(1);
                }

                promoteQueueToIntern(app, queue, currentSockets, i);
                if (strcmp(app->ext.id, app->self.id) == 0)
                    promoteInternToExtern(app, buffer);
                else
                {
                    memset(buffer, 0, MAX_BUFFER_SIZE);
                    sprintf(buffer, "EXTERN %s %s %s\n", app->ext.id, app->ext.ip, app->ext.port);
                    //printf("sent: %s\n", buffer);
                    if (writeTcp(app->interns.intr[app->interns.numIntr - 1].fd, buffer) < 0)
                    {
                        printf("Can't write when someone is trying to connect\n");
                        exit(1);
                    }
                }
            }
        }
    }
}