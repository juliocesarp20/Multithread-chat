#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFSZ 2048
#define NUMCLIENTS 15

int clientIds[NUMCLIENTS];

char sendingMessage[2048];

const char *errorMessages[] = {
    "",
    "User limit exceeded",
    "User not found",
    "Receiver not found"};

enum ErrorCodes getErrorCode(int value)
{
    if (value >= USER_LIMIT_EXCEEDED && value <= RECEIVER_NOT_FOUND)
    {
        return (enum ErrorCodes)value;
    }
    return 0; // Return a default error code or handle the case as needed
}

void clientUsage(int argc, char **argv)
{
    printf("usage: %s <server IP> <server port>\n", argv[0]);
    exit(EXIT_FAILURE);
}
//Extract input and get the struct
struct MSGTypeStruct extractIdAndMessage(const char *input, enum MSGS type)
{
    struct MSGTypeStruct msgs;
    msgs.type = NONE;
    msgs.idReceiver = -1;
    if (type == NONE)
        return msgs;

    if (type == TO)
    {
        const char *pattern = "send to %d \"%[^\"]\"";
        int iduser;
        char message[2048];
        sscanf(input, pattern, &iduser, message);
        msgs.idReceiver = iduser;
        strcpy(msgs.Message, message);
        msgs.type = TO;
        return msgs;
    }

    if (type == ALL)
    {
        const char *pattern = "send all \"%[^\"]\"";
        int iduser;
        char message[2048];
        sscanf(input, pattern, &iduser, message);
        strcpy(msgs.Message, message);
        msgs.type = ALL;
        return msgs;
    }
}
//Aux function to order
int compareIntegers(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

void printUsers()
{
    int start = 1;
    int end = 15;
    int numElements = end - 1;

    int copiedNumbers[numElements];

    for (int i = start-1; i < numElements; i++) {
        copiedNumbers[i] = clientIds[i+1];
    }

    qsort(copiedNumbers, numElements, sizeof(int), compareIntegers);

    for (int i = 0; i < numElements; i++) {
        if(copiedNumbers[i]!=-1)
            printf("%02d ", copiedNumbers[i]);
    }
    if(clientIds[1]!=-1)
        printf("\n");
}

void *receiveMessages(void *arg)
{
    int s = *((int *)arg);
    struct MessageStruct buffer;
    ssize_t count;

    while (1)
    {
        count = recv(s, &buffer, sizeof(struct MessageStruct), 0);
        if (count == -1)
        {
            perror("recv");
            exit(EXIT_FAILURE);
        }
        else if (count == 0)
        {
            // Connection closed by the server
            break;
        }

        switch (buffer.IdMsg)
        {
        //If it's a message from the active user print it, else add the user to buffer
        case MSG:
            if (clientIds[0] == buffer.IdSender)
            {
                puts(buffer.Message);
            }
            else
            {
                for (int i = 0; i < NUMCLIENTS; i++)
                {
                    if (clientIds[i] == buffer.IdSender)
                    {
                        puts(buffer.Message);
                        break;
                    }
                    if (clientIds[i] == -1)
                    {
                        clientIds[i] = buffer.IdSender;
                        printf("%s\n", buffer.Message);
                        break;
                    }
                }
            }

            break;
        //Add user list to buffer
        case RES_LIST:
            char *token = strtok(buffer.Message, ",");
            int i = 1;
            while (token != NULL)
            {
                int number = atoi(token);
                token = strtok(NULL, ",");
                clientIds[i] = number;
                i++;
            }
            break;
        //Convert error to message and print
        case ERROR:
            int errorCode = getErrorCode(atoi(buffer.Message));
            puts(errorMessages[errorCode]);
            if (errorCode == USER_LIMIT_EXCEEDED)
            {
                close(s);
                exit(EXIT_SUCCESS);
            }
            break;
        //User was removed, close connection
        case OK:
            if(strcmp(buffer.Message, "01") == 0){
            puts("Removed Successfully");
            close(s);
            exit(EXIT_SUCCESS);
            }else{
                puts(sendingMessage);
            }
            break;
        //Another user was removed, remove it from buffer
        case REQ_REM:
            for (int i = 0; i < NUMCLIENTS; i++)
            {
                if (clientIds[i] == buffer.IdSender)
                {
                    clientIds[i] = -1;
                    printf("User %02d left the group!\n", buffer.IdSender);
                    break;
                }
            }
            break;
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    // Check if the required command-line arguments are provided
    if (argc < 3)
    {
        clientUsage(argc, argv);
    }

    // Parse the server IP address and port number
    struct sockaddr_storage storage;
    if (0 != addrparse(argv[1], argv[2], &storage))
    {
        clientUsage(argc, argv);
        exit(EXIT_FAILURE);
    }

    // Create a socket
    int s;
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    if (s == -1)
    {
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != connect(s, addr, sizeof(storage)))
    {
        exit(EXIT_FAILURE);
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);

    struct MessageStruct buffer;

    for (int i = 0; i < NUMCLIENTS; i++)
    {
        clientIds[i] = -1;
    }

    size_t count;
    char buf[BUFSZ];

    struct MessageStruct msg;
    msg.IdMsg = 1;
    count = send(s, &msg, sizeof(struct MessageStruct), 0);

    if (count == -1)
    {
        exit(EXIT_FAILURE);
    }

    pthread_t recvThread;

    // Create a thread to receive messages from the server
    if (pthread_create(&recvThread, NULL, receiveMessages, &s) != 0)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        // Read user input
        fgets(buf, BUFSZ - 1, stdin);

        int option = (int)getClientOptions(buf);

        if(option==-1){
            option = REQ_REM;
        }

        switch (option)
        {
        case REQ_REM:
            resetMessageStruct(&msg);
            msg.IdMsg = REQ_REM;
            msg.IdSender = clientIds[0];
            break;

        case MSG:
            resetMessageStruct(&msg);
            enum MSGS type = isPattern(buf);

            if (type != NONE)
            {
                struct MSGTypeStruct msgTypes;
                msgTypes = extractIdAndMessage(buf, type);

                if (msgTypes.type == ALL)
                {
                    msg.IdMsg = MSG;
                    msg.IdSender = clientIds[0];
                    strcpy(msg.Message, msgTypes.Message);
                }
                else if (msgTypes.type == TO)
                {
                    msg.IdMsg = MSG;
                    msg.IdSender = clientIds[0];
                    msg.IdReceiver = msgTypes.idReceiver;
                    strcpy(msg.Message, msgTypes.Message);
                    strcpy(sendingMessage,msgTypes.Message);
                }
            }

            break;
        case RES_LIST:
            printUsers();
        break;
        }
        if(option!=RES_LIST){
        count = send(s, &msg, sizeof(struct MessageStruct), 0);

        if (count == -1)
        {
            exit(EXIT_FAILURE);
        }
        }

    }

    // Wait for the receive thread to finish
    pthread_join(recvThread, NULL);

    // Close the socket
    close(s);

    exit(EXIT_SUCCESS);
}
