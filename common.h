#ifndef COMMON_H
#define COMMON_H
#pragma once

#include <stdlib.h>

#include <arpa/inet.h>

// Supported options
enum Options
{
    REQ_ADD = 1,
    REQ_REM = 2,
    RES_LIST = 4,
    MSG = 6,
    ERROR = 7,
    OK = 8
};

enum MSGS
{
    TO = 9,
    ALL = 10,
    NONE = 0
};

enum ErrorCodes
{
    USER_LIMIT_EXCEEDED = 1,
    USER_NOT_FOUND = 2,
    RECEIVER_NOT_FOUND = 3
};

struct MessageStruct
{
    int IdMsg;
    int IdSender;
    int IdReceiver;
    char Message[2048];
};

struct MSGTypeStruct
{
    enum MSGS type;
    int idReceiver;
    char Message[2048];
};

enum MSGS isPattern(const char *input);

enum Options getClientOptions(const char *option);

int addrparse(const char *addrstr, const char *portstr,
              struct sockaddr_storage *storage);

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);

int server_sockaddr_init(const char *proto, const char *portstr,
                         struct sockaddr_storage *storage);

void resetMessageStruct(struct MessageStruct *msg);

#endif