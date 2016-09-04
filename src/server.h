#ifndef _SERVER_H_
#define _SERVER_H_

#include "common.h"

#define MAXBUFSEND 510
#define MAXBUFRECV 12
#define MAXCLIENTS 99
#define SLEEPTIME 10
#define SCORE 100
#define PENALTY 50
#define AWARD 20
#define MAINPT -1
#define ARGS 2
#define CLIENTA 'A'
#define CLIENTB 'B'
#define TASK 'T'
#define UP 'W'
#define DOWN 'S'
#define LEFT 'A'
#define RIGHT 'D'
#define COLLISION 'C'
#define TURNS 'S'
#define TURNL 'L'
#define TURNR 'R'
#define OK 'a'
#define TURNLMSG "L - Left   \0"
#define TURNRMSG "R - Right  \0"
#define HELLOAMSG "A - Client \0"
#define HELLOBMSG "B - Client \0"

struct connection
{
	int free;
	int score;
	int status;	
	int x;
	int y;
	int prevX;	// also targetX for client B - task
	int prevY;	// also targetY for client B - task
	int collision;		
	int taxiId;	// also taskId for client A - taxi
	int shouldWait;
	char turnLR;
	char direction;	
	char type;
	struct sockaddr_in addr;
};

struct threadArg
{
	int *fd;
	int assignedClient;
	int *cond_var;
	struct connection *connections;
	pthread_cond_t *cond;
	pthread_mutex_t *mutex;
};

void *xmalloc(size_t size);
int  bind_inet_socket(uint16_t port, int type);
char randDirection(int x, int y);
int  findFreeIndex(struct sockaddr_in addr, struct connection *connections);
int  checkIndex   (struct sockaddr_in addr, struct connection *connections);
void updateStatus (struct sockaddr_in addr, struct connection *connections, pthread_cond_t *cond, pthread_mutex_t *mutex, int *cond_var);
void turnLeftRight(struct sockaddr_in addr, struct connection *connections, pthread_cond_t *cond, pthread_mutex_t *mutex, int *cond_var, char lr);
void setTaskXY    (struct sockaddr_in addr, struct connection *connections, pthread_cond_t *cond, pthread_mutex_t *mutex, int *cond_var, char *buf);
void wakeUpThreads(                         struct connection *connections, pthread_cond_t *cond, pthread_mutex_t *mutex, int *cond_var);
void addNewClient (struct sockaddr_in addr, struct threadArg *ptArg, char type);
int  randPosition (int clientId,            struct connection *connections);
int  threadUpdate (int clientId,            struct connection *connections);
int  threadUpdateB(int clientId,            struct connection *connections);
int  threadUpdateA(int clientId,            struct connection *connections);
void threadUpdateACollision(int clientId,   struct connection *connections);
char threadUpdateADirection(char direction, char turnLR);
char threadUpdateADirectionEdge(char oldDirection, char direction, int x, int y);
int  threadSend   (int clientId,            struct connection *connections, int fd);
int  threadSendA  (int clientId,            struct connection *connections, int fd);
int  threadSendB  (int clientId,            struct connection *connections, int fd);
void *threadClient(void *ptArg);
void doServer(int fd);

#endif
