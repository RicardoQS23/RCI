#include "project.h"

void handleInterruptions(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName)
{
    handleUserInputInterruption(app, readSockets, currentSockets, cmd, bootIP, name, dest, bootID, bootTCP, net, regIP, regUDP, fileName);
    handleServerInterruption(app, queue, readSockets, currentSockets);
    handleExtInterruption(app, readSockets, currentSockets);
    handleInternInterruptions(app, readSockets, currentSockets);
    handleQueueInterruptions(app, queue, readSockets, currentSockets);
}


void handleUserInputInterruption(AppNode *app, fd_set *readSockets, fd_set *currentSockets, enum commands *cmd, char *bootIP, char *name, char *dest, char *bootID, char *bootTCP, char *net, char *regIP, char *regUDP, char *fileName)
{
    if (FD_ISSET(0, readSockets))
    {
        char buffer[MAX_BUFFER_SIZE] = "\0";
        FD_CLR(0, readSockets);
        if(fgets(buffer, MAX_BUFFER_SIZE, stdin) == NULL)
        {
            printf("Couldn't read user input\n");
            return;
        }
        if (validateUserInput(app, cmd, buffer, bootIP, name, dest, bootID, bootTCP, net, fileName) < 0)
        {
            printf("bad user input\n");
            return;
        }
        commandMultiplexer(app, *cmd, currentSockets, buffer, bootIP, name, dest, bootID, bootTCP, net, regIP, regUDP, fileName);
    }
}

void handleServerInterruption(AppNode *app, NodeQueue *queue, fd_set *readSockets, fd_set *currentSockets)
{
    if (FD_ISSET(app->self.socket.fd, readSockets))
    {
        FD_CLR(app->self.socket.fd, readSockets);
        acceptNeighbourConnection(app, queue, currentSockets);
    }
}

void handleExtInterruption(AppNode *app, fd_set *readSockets, fd_set *currentSockets)
{
    if (FD_ISSET(app->ext.socket.fd, readSockets)) // ext talks with us
    {
        FD_CLR(app->ext.socket.fd, readSockets);
        if (readTcp(&(app->ext.socket)) < 0) // conexao fechou e temos de estabelecer nova
        {
            closedExtConnection(app, currentSockets);
        }
        else
        {
            handleCommunication(app, &(app->ext));
        }
    }
}

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