#include "client_common.h"
#include "common.h"
#include "client.h"

void printBoard(char * buf)
{
	int i, j, k = 1, clientId, score, taxiId;
	char cell[4];
	if (buf[0] == SERVTERM)
	{
		fprintf(stdout, "\nServer has terminated. End of the game.\n");
		xfflush(stdout, 0);
		if (kill(0, SIGINT)) 
			ERR("kill");
		return;
	}
	if (buf[0] == LOOSE)
	{
		fprintf(stdout, "\nYou lose the game. End of the game.\n");
		xfflush(stdout, 0);
		if (kill(0, SIGINT)) 
			ERR("kill");
		return;
	}
	clientId = 10 * (buf[1] - '0') + (buf[2] - '0');
	taxiId = 10 * (buf[3] - '0') + (buf[4] - '0') - 1;
	score = 100 * (buf[5] - '0') + 10 * (buf[6] - '0') + (buf[7] - '0');
	fprintf(stdout, "\nYour id: %d, score: %d, task id: %d\n", clientId, score, taxiId);
	xfflush(stdout, 0);
	for (i = 0; i < ROADS; ++i)
	{
		for (j = 0; j < ROADS; ++j)
		{	
			cell[0] = EMPTY; cell[1] = EMPTY; cell[3] = EMPTY;
			for(k = 8; k < MAXBUFRECV - 4; k = k + 5)
				if (buf[k + 2] == j + '0' && buf[k + 3] == i + '0')
				{
					cell[0] = buf[k]; cell[1] = buf[k + 1]; cell[3] = buf[k + 4];
					if (clientId == 10 * (buf[k] - '0') + (buf[k + 1] - '0') || taxiId == 10 * (buf[k] - '0') + (buf[k + 1] - '0'))
						break;
				}
			fprintf(stdout, "%c%c_%c ", cell[0], cell[1], cell[3]);
		}
		fprintf(stdout, "\n");
	}
	xfflush(stdout, 0);
}

int get_stdin(char *buf)
{
	int status;
	status = TEMP_FAILURE_RETRY(read(STDIN_FILENO, buf, 1));
	if(status < 0) 
		ERR("read:");
	if((0 == status) || ('\n' == *buf))
	{  
		memset(buf, 0, 1);
		return 0;
	}
	return status;
}

void doClient(int fd, struct sockaddr_in addr)
{	
	fd_set base_rfds, rfds;
	int success = 0, maxfd;
	char bufsend[MAXBUFSEND];
	char bufrecv[MAXBUFRECV];
	char bufturn[1];
	FD_ZERO(&base_rfds);
	FD_SET(fd, &base_rfds);
	FD_SET(STDIN_FILENO, &base_rfds);
	maxfd = fd > STDIN_FILENO ? fd + 1 : STDIN_FILENO + 1;
	strncpy(bufsend, HELLOAMSG, MAXBUFSEND);
	success = sendAndConfirm(fd, bufsend, bufrecv, addr);
	if (success)
	{
		alarm(STATUSTIME);
		while (last_signal != SIGINT)
		{
			rfds = base_rfds;
			if (last_signal == SIGALRM)
			{
				strncpy(bufsend, STATUSMSG, MAXBUFSEND);	
				if(TEMP_FAILURE_RETRY(sendto(fd, bufsend, MAXBUFSEND, 0, &addr, sizeof(addr))) < 0) 
					ERR("sendto:");
				if (last_signal != SIGINT)
				{
					last_signal = 0;
					alarm(STATUSTIME);
				}
			}
			if(select(maxfd, &rfds, NULL, NULL, NULL) > 0)
			{
				if(FD_ISSET(STDIN_FILENO, &rfds))
					if(get_stdin(bufturn))
					{
						if (bufturn[0] == TURNL)
						{
							strncpy(bufsend, TURNLMSG, MAXBUFSEND);		
							if(TEMP_FAILURE_RETRY(sendto(fd, bufsend, MAXBUFSEND, 0, &addr, sizeof(addr))) < 0) 
								ERR("sendto:");
						}
						else if(bufturn[0] == TURNR)
						{
							strncpy(bufsend, TURNRMSG, MAXBUFSEND);
							if(TEMP_FAILURE_RETRY(sendto(fd, bufsend, MAXBUFSEND, 0, &addr, sizeof(addr))) < 0) 
								ERR("sendto:");
						}
					}
				if(FD_ISSET(fd, &rfds))
				{
					if(last_signal != SIGINT && recv(fd, bufrecv, MAXBUFRECV, 0) < 0)
					{
						if(EINTR != errno)
							ERR("recv:");
						if(SIGINT == last_signal)
							break;
					}
					printBoard(bufrecv);
				}
			}
			else if(EINTR != errno) 
				ERR("select");	
		}
	}
}

int main(int argc, char** argv) 
{
	int fd;
	struct sockaddr_in addr;
	if(argc != ARGS) 
	{
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if(sethandler(SIG_IGN, SIGPIPE)) 
		ERR("Seting SIGPIPE:");
	if(sethandler(signal_handler, SIGINT)) 
		ERR("Seting SIGINT:");
	if(sethandler(signal_handler, SIGALRM)) 
		ERR("Seting SIGALRM:");
	fd = make_socket(PF_INET, SOCK_DGRAM);
	addr = make_address(argv[1], atoi(argv[2]));
	doClient(fd, addr);
	if(TEMP_FAILURE_RETRY(close(fd))<0)
		ERR("close");
	fprintf(stdout, "Client terminates\n");
	xfflush(stdout, 0);
	pthread_exit(EXIT_SUCCESS);
}
