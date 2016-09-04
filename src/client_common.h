#ifndef _CLIENT_COMMON_H_
#define _CLIENT_COMMON_H_

#include "common.h"

#define MAXBUFSEND 12
#define MAXBUFRECV 510
#define ARGS 3
#define STATUSTIME 3
#define CONNECTIONTIME 10
#define TRYCONNECT 3

struct sockaddr_in make_address(char *address, uint16_t port);
int sendAndConfirm(int fd, char *bufSend, char *bufRecv, struct sockaddr_in addr);
void printBoard(char *buf);
void doClient(int fd, struct sockaddr_in addr);

#endif 
