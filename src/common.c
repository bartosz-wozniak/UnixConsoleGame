#include "common.h"

volatile sig_atomic_t last_signal = 0;

void signal_handler(int sig) 
{
	if (last_signal != SIGINT)
		last_signal = sig;
}

int sethandler(void (*f)(int), int sigNo)
{
	struct sigaction act; 
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1 == sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

int xfflush(FILE * file, int b_fatal)
{
	while(EOF == fflush(file))
	{
		if(errno == EINTR)
			continue;
		perror("fflush failed");
		if(b_fatal)
			exit(EXIT_FAILURE);
		return -1;
	}
	return 0;
}

void usage(char * name)
{
	fprintf(stderr,"USAGE: %s [client: domain] port\n", name);
}

int make_socket(int domain, int type)
{
	int sock = socket(domain, type, 0);
	if(sock < 0) 
		ERR("socket");
	return sock;
}
