#ifndef _COMMON_H_
#define _COMMON_H_

#define _GNU_SOURCE
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#define ROADS 10
#define EMPTYN -1
#define EMPTY '_'
#define STATUSMSG "S - Status \0"
#define CONNERR 'd'
#define SERVTERM 'c'
#define LOOSE 'b'

#define ERR(source)\
	(\
		perror(source),\
		fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),\
		exit(EXIT_FAILURE)\
	)

extern volatile sig_atomic_t last_signal;

void signal_handler(int sig);
int sethandler(void (*f)(int), int sigNo);
int xfflush(FILE *file, int b_fatal);
void usage(char *name);
int make_socket(int domain, int type);

#endif 
