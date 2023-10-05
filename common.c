#include "common.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

enum MSGS isPattern(const char *input)
{
    const char *pattern = "send to %d \"%[^\"]\"";
    int iduser;
    char message[2048];

    if (sscanf(input, pattern, &iduser, message) == 2)
    {
        return TO;
    }
    const char *patternAll = "send all \"%[^\"]\"";
    if (sscanf(input, patternAll, message) == 1)
    {
        return ALL;
    }
    return NONE;
}

enum Options getClientOptions(const char *option)
{
    char temp[BUFSIZ];
    strncpy(temp, option, sizeof(temp));
    temp[sizeof(temp) - 1] = '\0';

    size_t length = strcspn(temp, "\n");
    temp[length] = '\0';

    if (strcmp(temp, "close connection") == 0)
        return REQ_REM;

    if (strcmp(temp, "list users") == 0)
        return RES_LIST;

    if ((isPattern(temp)!=NONE))
    {
        return MSG;
    }
    return -1;
}

void resetMessageStruct(struct MessageStruct *msg)
{
    msg->IdMsg = 0;                                
    msg->IdSender = 0;                             
    msg->IdReceiver = 0;                          
    memset(msg->Message, 0, sizeof(msg->Message)); // Reset the char array
}

/*
 * Parse the address and port strings and initialize sockaddr.
 * Returns 0 on success, -1 on failure.
 */
int addrparse(const char *addrstr, const char *portstr, struct sockaddr_storage *storage)
{
    if (addrstr == NULL || portstr == NULL)
    {
        exit(EXIT_FAILURE);
    }

    uint16_t port = (uint16_t)atoi(portstr);

    if (port == 0)
    {
        return -1;
    }
    port = htons(port);

    struct in_addr inaddr4;
    if (inet_pton(AF_INET, addrstr, &inaddr4))
    {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_port = port;
        addr4->sin_addr = inaddr4;
        return 0;
    }

    struct in6_addr inaddr6;
    if (inet_pton(AF_INET6, addrstr, &inaddr6))
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = port;
        memcpy(&(addr6->sin6_addr), &inaddr6, sizeof(inaddr6));
        return 0;
    }

    return -1;
}

/*
 * Convert the sockaddr to a string.
 * The result is stored in the 'str' parameter.
 */
void addrtostr(const struct sockaddr *addr, char *str, size_t strsize)
{
    int version;
    char addrstr[INET6_ADDRSTRLEN + 1] = "";
    uint16_t port;

    if (addr->sa_family == AF_INET)
    {
        version = 4;
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        if (!inet_ntop(AF_INET, &(addr4->sin_addr), addrstr, INET6_ADDRSTRLEN + 1))
        {
            exit(EXIT_FAILURE);
        }
        port = ntohs(addr4->sin_port);
    }
    else if (addr->sa_family == AF_INET6)
    {
        version = 6;
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        if (!inet_ntop(AF_INET6, &(addr6->sin6_addr), addrstr, INET6_ADDRSTRLEN + 1))
        {
            exit(EXIT_FAILURE);
        }
        port = ntohs(addr6->sin6_port);
    }
    else
    {
        exit(EXIT_FAILURE);
    }

    if (str)
    {
        snprintf(str, strsize, "IPv%d %s %hu", version, addrstr, port);
    }
}

/*
 * Initialize the sockaddr_storage structure.
 * The 'proto' parameter specifies the protocol ("v4" or "v6").
 * The 'portstr' parameter specifies the port number.
 * Returns 0 on success, -1 on failure.
 */
int server_sockaddr_init(const char *proto, const char *portstr, struct sockaddr_storage *storage)
{
    uint16_t port = (uint16_t)atoi(portstr);
    if (port == 0)
    {
        return -1;
    }
    port = htons(port);

    memset(storage, 0, sizeof(*storage));

    if (0 == strcmp(proto, "v4"))
    {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_addr.s_addr = INADDR_ANY;
        addr4->sin_port = port;
        return 0;
    }
    else if (0 == strcmp(proto, "v6"))
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_addr = in6addr_any;
        addr6->sin6_port = port;
        return 0;
    }
    else
    {
        return -1;
    }
}