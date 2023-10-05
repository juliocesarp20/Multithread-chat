#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include "common.h"

#define BUFFSZ sizeof(struct MessageStruct)
#define NUMCLIENTS 15

// Global variables
int clientSockets[NUMCLIENTS];
int clientIds[NUMCLIENTS];
int totalClients = 0;

pthread_mutex_t clientsMutex; // Mutex for accessing clientSockets and numClients

void serverUsage(int argc, char **argv)
{
    printf("usage: %serverSocket <v4 or v6> <server port>\n", argv[0]);
    printf("example: %serverSocket v4 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

/*Generate timestamp and construct the new message format
iterate through the clientSockets array and send the message to each connected client*/
void sendBroadcastMessage(struct MessageStruct buffer, int idSender)
{
    char times[10];
    char id[3];
    char msg[2048];
    time_t t;
    t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(times, sizeof(times), "[%H:%M]", &tm);
    char newMessage[2048];
    strcpy(msg, buffer.Message);
    for (int i = 0; i < NUMCLIENTS; i++)
    {
        if (clientIds[i] != -1)
        {
            if (idSender > 0)
            {

                if (clientIds[i] == idSender)
                {
                    strcpy(newMessage, "");
                    strcat(newMessage, times);
                    strcat(newMessage, " -> all: ");
                    strcat(newMessage, msg);
                    strcpy(buffer.Message, newMessage);
                }
                else
                {
                    strcpy(newMessage, "");
                    strcat(newMessage, times);
                    strcat(newMessage, " ");
                    sprintf(id, "%02d", buffer.IdSender);
                    strcat(newMessage, id);
                    strcat(newMessage, ": ");
                    strcat(newMessage, msg);
                    strcpy(buffer.Message, newMessage);
                }
            }
            ssize_t bytesSent = send(clientSockets[i], &buffer, sizeof(buffer), 0);
            if (bytesSent == -1)
            {
                perror("Error writing to socket");
            }
        }
    }
}

// Function executed by each thread
void *clientHandler(void *arg)
{
    int clientSocket = *((int *)arg);
    struct MessageStruct buffer;

    while (1)
    {
        ssize_t bytesRead = read(clientSocket, &buffer, sizeof(struct MessageStruct));
        if (bytesRead == -1 || bytesRead == 0)
        {
            close(clientSocket);
            pthread_exit(NULL);
        }

        pthread_mutex_lock(&clientsMutex);
        switch (buffer.IdMsg)
        {
        //Adds a new client and send a broadcast message saying it was added
        case REQ_ADD:

            if (totalClients < NUMCLIENTS)
            {
                for (int i = 0; i < NUMCLIENTS; i++)
                {
                    if (clientIds[i] == -1)
                    {
                        resetMessageStruct(&buffer);
                        buffer.IdMsg = MSG;
                        buffer.IdSender = i + 1;
                        snprintf(buffer.Message, sizeof(buffer.Message), "User %02d joined the group!", i + 1);
                        totalClients++;
                        clientSockets[i] = clientSocket;
                        clientIds[i] = i + 1;
                        sendBroadcastMessage(buffer, 0);
                        printf("User %02d added\n", i + 1);

                        resetMessageStruct(&buffer);
                        buffer.IdMsg = RES_LIST;

                        int length = 0;
                        for (int j = 0; j < NUMCLIENTS; j++)
                        {
                            if (clientIds[j] != -1 && clientIds[j] != (i + 1))
                            {
                                int idLength = snprintf(NULL, 0, "%d,", clientIds[j]);
                                char idString[idLength + 1];
                                snprintf(idString, sizeof(idString), "%d,", clientIds[j]);
                                strcat(buffer.Message, idString);
                                length += idLength;
                            }
                        }

                        if (length > 0)
                        {
                            buffer.Message[length - 1] = '\0';
                        }

                        ssize_t bytesSent = send(clientSocket, &buffer, sizeof(buffer), 0);
                        if (bytesSent == -1)
                        {
                            perror("Error writing to socket");
                        }

                        break;
                    }
                }
            }
            else
            {
                //Server is full
                resetMessageStruct(&buffer);
                buffer.IdMsg = ERROR;
                strcpy(buffer.Message, "01");
                ssize_t bytesSent = send(clientSocket, &buffer, sizeof(buffer), 0);
                if (bytesSent == -1)
                {
                    perror("Error writing to socket");
                }
                pthread_mutex_unlock(&clientsMutex);
                pthread_exit(NULL);
            }
            break;
        //Removes a client and send a broadcast saying it was removed
        case REQ_REM:
            int userFound = 0;
            // Remove client socket from the array
            int userId = buffer.IdSender;
            for (int i = 0; i < NUMCLIENTS; i++)
            {
                if (clientSockets[i] == clientSocket)
                {
                    // Shift the remaining clients
                    clientSockets[i] = -1;
                    clientIds[i] = -1;
                    totalClients--;
                    userFound = 1;
                    resetMessageStruct(&buffer);
                    buffer.IdMsg = OK;
                    strcpy(buffer.Message, "01");
                    ssize_t bytesSent = send(clientSocket, &buffer, sizeof(buffer), 0);
                    if (bytesSent == -1)
                    {
                        perror("Error writing to socket");
                    }
                    break;
                }
            }

            if (userFound == 0)
            {
                //If client not found send a error message
                resetMessageStruct(&buffer);
                buffer.IdMsg = ERROR;
                buffer.IdReceiver = userId;
                strcpy(buffer.Message, "02");
                ssize_t bytesSent = send(clientSocket, &buffer, sizeof(buffer), 0);
                if (bytesSent == -1)
                {
                    perror("Error writing to socket");
                }
            }
            else
            {
                ///Else send message saying it was removed
                printf("User %02d removed\n",userId);
                resetMessageStruct(&buffer);
                close(clientSocket);
                buffer.IdMsg = REQ_REM;
                buffer.IdSender = userId;
                sendBroadcastMessage(buffer, 0);
                pthread_mutex_unlock(&clientsMutex);
                pthread_exit(NULL);
            }
            break;
        //If receives command to message check if there is idReceiver and idSender
        case MSG:
            if (buffer.IdReceiver > 0)
            {
                int socketSenderSuccess = 0;
                char msg[2048];
                strcpy(msg, buffer.Message);
                int userExists = 0;
                if (buffer.IdSender != buffer.IdReceiver)
                {
                    for (int i = 0; i < NUMCLIENTS; i++)
                    {
                        if (buffer.IdReceiver == clientIds[i])
                        {
                            //If client found make the timestamp and send the message
                            char times[10];
                            char id[3];

                            time_t t;
                            t = time(NULL);
                            struct tm tm = *localtime(&t);
                            strftime(times, sizeof(times), "[%H:%M]", &tm);
                            char newMessage[2048];

                            strcpy(newMessage, "");
                            strcat(newMessage, "P ");
                            strcat(newMessage, times);
                            strcat(newMessage, " ");
                            sprintf(id, "%02d", buffer.IdSender);
                            strcat(newMessage, id);
                            strcat(newMessage, ": ");
                            strcat(newMessage, msg);
                            strcpy(buffer.Message, newMessage);
                            userExists = 1;

                            buffer.IdMsg = MSG;
                            ssize_t bytesSent = send(clientSockets[i], &buffer, sizeof(buffer), 0);
                            if (bytesSent == -1)
                            {
                                perror("Error writing to socket");
                            }
                        }
                        else if (buffer.IdSender == clientIds[i])
                        {
                            socketSenderSuccess = clientSockets[i];
                        }
                    }
                }
                if (!userExists)
                {
                    //If client not found send error message
                    printf("User %02d not found\n",buffer.IdReceiver);
                    userId = buffer.IdSender;
                    resetMessageStruct(&buffer);
                    buffer.IdMsg = ERROR;
                    buffer.IdReceiver = userId;
                    strcpy(buffer.Message, "03");
                    ssize_t bytesSent = send(clientSocket, &buffer, sizeof(buffer), 0);
                    if (bytesSent == -1)
                    {
                        perror("Error writing to socket");
                    }
                }
                else
                {
                    //send the private message sent indicator
                    char times[10];
                    char id[3];

                    time_t t;
                    t = time(NULL);
                    struct tm tm = *localtime(&t);
                    strftime(times, sizeof(times), "[%H:%M]", &tm);
                    char newMessage[2048];

                    strcpy(newMessage, "");
                    strcat(newMessage, "P ");
                    strcat(newMessage, times);
                    strcat(newMessage, " -> ");
                    sprintf(id, "%02d", buffer.IdReceiver);
                    strcat(newMessage, id);
                    strcat(newMessage, ": ");
                    strcat(newMessage, msg);
                    strcpy(buffer.Message, newMessage);
                    userExists = 1;

                    ssize_t bytesSent = send(socketSenderSuccess, &buffer, sizeof(buffer), 0);
                    if (bytesSent == -1)
                    {
                        perror("Error writing to socket");
                    }
                }
            }
            else
            {
                //If it doesn't have receiver send a broadcast
                char times[10];
                char id[3];

                time_t t;
                t = time(NULL);
                struct tm tm = *localtime(&t);
                strftime(times, sizeof(times), "[%H:%M]", &tm);
                char newMessage[2048];

                strcpy(newMessage, "");
                strcat(newMessage, times);
                strcat(newMessage, " ");
                sprintf(id, "%02d", buffer.IdSender);
                strcat(newMessage, id);
                strcat(newMessage, ": ");
                strcat(newMessage,buffer.Message);
                puts(newMessage);

                
                sendBroadcastMessage(buffer, buffer.IdSender);
            }
            break;
        }
        pthread_mutex_unlock(&clientsMutex);
    }
}

int main(int argc, char *argv[])
{
    for (int i = 0; i < 15; i++)
    {
        clientIds[i] = -1;
    }
    int serverSocket;
    int clientSocket;
    pthread_t threadId;
    pthread_attr_t threadAttr;

    // Initialize the mutex
    if (pthread_mutex_init(&clientsMutex, NULL) != 0)
    {
        perror("Mutex initialization failed");
        exit(EXIT_FAILURE);
    }

    if (argc < 3)
    {
        serverUsage(argc, argv);
    }


    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(argv[1], argv[2], &storage))
    {
        serverUsage(argc, argv);
    }

    serverSocket = socket(storage.ss_family, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        exit(EXIT_FAILURE);
    }

    int enable = 1;
    if (0 != setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)))
    {
        exit(EXIT_FAILURE);
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);

    if (0 != bind(serverSocket, addr, sizeof(storage)))
    {
        exit(EXIT_FAILURE);
    }


    if (0 != listen(serverSocket, 10))
    {
        exit(EXIT_FAILURE);
    }

    char addrstr[BUFFSZ];

    addrtostr(addr, addrstr, BUFFSZ);

    // Initialize thread attributes
    pthread_attr_init(&threadAttr);
    pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);
    struct MessageStruct buffer;
    while (1)
    {
        clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == -1)
        {
            perror("Error accepting connection");
            close(serverSocket);
            exit(EXIT_FAILURE);
        }

        // Create a new thread to handle the client
        if (pthread_create(&threadId, &threadAttr, clientHandler, &clientSocket) != 0)
        {
            perror("Error creating thread");
            close(clientSocket);
            continue;
        }
    }

    //clean usages
    close(serverSocket);
    pthread_attr_destroy(&threadAttr);
    pthread_mutex_destroy(&clientsMutex);

    return 0;
}
