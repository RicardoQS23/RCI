#include "project.h"

int chooseRandomNodeToConnect(char *buffer, char *my_id)
{
    char buffer_cpy[MAX_BUFFER_SIZE];
    char *token, id[3];
    int counter = 0, i, num, nodeOnNet = 0;
    srand(time(0));

    memset(buffer_cpy, 0, MAX_BUFFER_SIZE);
    strcpy(buffer_cpy, buffer);
    token = strtok(buffer_cpy, "\n");
    token = strtok(NULL, "\n");
    while (token != NULL)
    {
        if(sscanf(token, "%s %*s %*s", id) == 1)
        {
            if(strcmp(id, my_id) == 0)
                nodeOnNet = 1;
        }
        counter++;
        token = strtok(NULL, "\n");
    }

    if (counter == 0) // lista de nos na rede vazia
    {
        return 0;
    }

    token = strtok(buffer, "\n");
    token = strtok(NULL, "\n");

    num = (rand() % counter);
    for (i = 0; i < counter - 1; i++)
    {
        if (i == num)
            break;
        token = strtok(NULL, "\n");
    }
    strcpy(buffer, token);
    return nodeOnNet;
}

void udpClient(char *buffer, char *ip, char *port, char *net)
{
    int errcode, fd;
    ssize_t n;
    struct addrinfo hints, *res;
    socklen_t addrlen;
    struct sockaddr_in addr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
        exit(1);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    errcode = getaddrinfo(ip, port, &hints, &res);
    if (errcode != 0)
        exit(1);

    n = sendto(fd, buffer, strlen(buffer) + 1, 0, res->ai_addr, res->ai_addrlen);
    if (n == -1)
        exit(1);

    addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&addr, &addrlen);
    if (n == -1)
        exit(1);
    printf("%s\n", buffer);
    freeaddrinfo(res);
    close(fd);
}

void regNetwork(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net)
{
    memset(buffer, 0, MAX_BUFFER_SIZE);
    sprintf(buffer, "REG %s %s %s %s", net, app->self.id, app->self.ip, app->self.port);
    //printf("sent: %s\n", buffer);
    udpClient(buffer, regIP, regUDP, net);
    FD_SET(app->self.fd, currentSockets);
}

void unregNetwork(AppNode *app, char *buffer, fd_set *currentSockets, char *regIP, char *regUDP, char *net)
{
    memset(buffer, 0, MAX_BUFFER_SIZE);
    sprintf(buffer, "UNREG %s %s", net, app->self.id);
    //printf("sent: %s\n", buffer);
    udpClient(buffer, regIP, regUDP, net);
}

void cleanQueue(NodeQueue *queue, fd_set *currentSockets)
{
    for (int i = queue->numNodesInQueue; i >= 0; i--)
    {
        FD_CLR(queue->queue[i].fd, currentSockets);
        close(queue->queue[i].fd);
    }
    memset(queue, 0, sizeof(NodeQueue));
}

void popQueue(NodeQueue *queue, fd_set *currentSockets, int pos)
{
    memmove(&queue->queue[pos], &queue->queue[queue->numNodesInQueue - 1], sizeof(NODE));
    memset(&queue->queue[queue->numNodesInQueue - 1], 0, sizeof(NODE));
    queue->numNodesInQueue--;
}

void promoteQueueToIntern(AppNode *app, NodeQueue *queue, fd_set *currentSockets, int pos)
{
    app->interns.numIntr++;
    memmove(&app->interns.intr[app->interns.numIntr - 1], &queue->queue[pos], sizeof(NODE));
    popQueue(queue, currentSockets, pos);
    updateExpeditionTable(app, app->interns.intr[app->interns.numIntr - 1].id, app->interns.intr[app->interns.numIntr - 1].id, app->interns.intr[app->interns.numIntr - 1].fd);
}