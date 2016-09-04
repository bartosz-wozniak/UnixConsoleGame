#include "common.h"
#include "client_common.h"

struct sockaddr_in make_address(char *address, uint16_t port)
{
	struct sockaddr_in addr;
	struct hostent *hostinfo;
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	hostinfo = gethostbyname(address);
	if(hostinfo == NULL)
		ERR("gethostbyname");
	addr.sin_addr = *(struct in_addr*) hostinfo -> h_addr;
	return addr;
}

int sendAndConfirm(int fd, char* bufSend, char* bufRecv, struct sockaddr_in addr)
{
	struct itimerval ts;
	int counter = 0;
	ssize_t rs;
	do
	{
		counter++;
		if(TEMP_FAILURE_RETRY(sendto(fd, bufSend, MAXBUFSEND, 0, &addr, sizeof(addr))) < 0) 
			ERR("sendto:");
		memset(&ts, 0, sizeof(struct itimerval));
		ts.it_value.tv_sec = CONNECTIONTIME;
		setitimer(ITIMER_REAL, &ts, NULL);
		if (last_signal != SIGINT)
			last_signal = 0;
		while((rs = recv(fd, bufRecv, MAXBUFRECV, 0)) < 0)
		{
			if(EINTR != errno) ERR("recv:");
			if(SIGALRM == last_signal) break;
			if(SIGINT == last_signal) return 0;
		}
	} while(rs < 0 && counter < TRYCONNECT && last_signal != SIGINT);
	if(rs < 0) 
	{
		fprintf(stdout, "\nServer not responding. End of the game.\n");
		xfflush(stdout, 0);
		return 0;
	}
	if (bufRecv[0] == CONNERR)
	{
		fprintf(stdout, "\nToo many connections. Server is busy. No place left. End of the game.\n");
		xfflush(stdout, 0);
		return 0;
	}
	printBoard(bufRecv);
	return 1;
}
