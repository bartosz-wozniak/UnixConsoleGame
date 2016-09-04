#include "server.h"
#include "common.h"

void * xmalloc (size_t size)
{
	register void *value = malloc (size);
	if (value == 0)
		ERR("malloc: ");
	return value;
}

int bind_inet_socket(uint16_t port, int type)
{
	struct sockaddr_in addr;
	int socketfd = make_socket(PF_INET, type), t = 1;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t))) 
		ERR("setsockopt");
	if(bind(socketfd, (struct sockaddr*) &addr, sizeof(addr)) < 0)  
		ERR("bind");
	return socketfd;
}

int findFreeIndex(struct sockaddr_in addr, struct connection *connections)
{
	int i, empty = EMPTYN;	
	for(i = 0; i < MAXCLIENTS; i++)
		if(connections[i].free)
		{ 
			empty = i;
			break;
		}
	if(empty != EMPTYN) 
	{
		connections[empty].free = 0;
		connections[empty].addr = addr;
	}
	return empty;
}

int checkIndex(struct sockaddr_in addr, struct connection *connections)
{
	int i, pos = EMPTYN;
	for(i = 0; i < MAXCLIENTS; i++)
		if(!connections[i].free && 0 == memcmp(&addr, 
			&(connections[i].addr), sizeof(struct sockaddr_in))) 
		{
			pos = i;
			break;
		}
	return pos;
}

char randDirection(int x, int y)
{
	char randDir[4] = { LEFT, RIGHT, UP, DOWN };
	if (x == 0)
		return RIGHT;
	if (x == ROADS - 1)
		return LEFT;
	if (y == 0)
		return DOWN;
	if (y == ROADS - 1)
		return UP;
	return randDir[(x + y) % 4];
}

int randPosition(int clientId, struct connection *connections)
{
	int i, j, k, canAdd = 1;
	if (connections[clientId].type != CLIENTA)
	{
		fprintf(stdout, "Error, randPosition, client B cannot rand starting position\n");
		xfflush(stdout, 0);
		return 0;
	}
	for (i = 0; i < ROADS; ++i)
		for (j = 0; j < ROADS; ++j)
		{
			canAdd = 1;
			for (k = 0; k < MAXCLIENTS; ++k)
				if (!connections[k].free && k != clientId && 
					connections[k].x != EMPTYN && connections[k].y != EMPTYN &&
					(abs(connections[k].x - j) + abs(connections[k].y - i) < 3))
				{
					canAdd = 0;
					break;
					
				}	
			if (canAdd)
			{
				connections[clientId].x = j;
				connections[clientId].y = i;
				connections[clientId].direction = randDirection(j, i);
				return 1;
			}
		}
	return 0;
}

void SendResponse(int fd, struct sockaddr_in addr)
{
	char buf[MAXBUFSEND];
	int k = 0;
	buf[k++] = CONNERR;
	while (k < MAXBUFSEND - 1)
		buf[k++] = EMPTY;
	buf[k] = '\0';
	if(TEMP_FAILURE_RETRY(sendto(fd, buf, MAXBUFSEND, 0, &addr, sizeof(addr))) < 0) 
		ERR("sendto:");
}

int threadUpdateB(int clientId, struct connection *connections)
{
	int i;
	if(connections[clientId].taxiId != EMPTYN)
	{
		connections[clientId].x = connections[connections[clientId].taxiId].x;
		connections[clientId].y = connections[connections[clientId].taxiId].y;
	}
	if(connections[clientId].status == 0 && connections[clientId].taxiId == EMPTYN)
	{	
		connections[clientId].free = 1;
		fprintf(stdout, "Client: %d not responding, disconnecting\n", clientId);
		xfflush(stdout, 0);
		return 0;
	}
	if(connections[clientId].taxiId == EMPTYN)
	{
		for (i = 0; i < MAXCLIENTS; ++i)
			if (!connections[i].free && connections[i].type == CLIENTA && 
				connections[i].taxiId == EMPTYN && connections[i].score > 0
				&& connections[i].x == connections[clientId].x && 
				connections[i].y == connections[clientId].y)
			{
				connections[clientId].taxiId = i;
				connections[i].taxiId = clientId;
				connections[i].shouldWait = 1;
				break;
			}
		fprintf(stdout, "Thread updated client B: %d\n", clientId);
		xfflush(stdout, 0);
		return 1;
	}
	if(connections[clientId].x == connections[clientId].prevX && 
		connections[clientId].y == connections[clientId].prevY && 
		connections[clientId].prevX != EMPTYN && 
		connections[clientId].prevY != EMPTYN)
	{	
		if(connections[clientId].taxiId != EMPTYN)
		{
			connections[connections[clientId].taxiId].score += AWARD;
			connections[connections[clientId].taxiId].taxiId = EMPTYN;
			connections[connections[clientId].taxiId].shouldWait = 1;
		}
		fprintf(stdout, "Client B: %d reached target\n", clientId);
		xfflush(stdout, 0);
		if(connections[clientId].status) return 1;
		else 
		{
			connections[clientId].free = 1;
			return 0;
		}
	}
	return 1;
}

char threadUpdateADirection(char direction, char turnLR)
{
	if (direction == UP && turnLR == TURNL)
		return LEFT;
	if (direction == UP && turnLR == TURNR)
		return RIGHT;
	if (direction == UP)
		return UP;
	if (direction == DOWN && turnLR == TURNL)
		return RIGHT;
	if (direction == DOWN && turnLR == TURNR)
		return LEFT;
	if (direction == DOWN)
		return DOWN;
	if (direction == LEFT && turnLR == TURNL)
		return DOWN;
	if (direction == LEFT && turnLR == TURNR)
		return UP;
	if (direction == LEFT)
		return LEFT;
	if (direction == RIGHT && turnLR == TURNL)
		return UP;
	if (direction == RIGHT && turnLR == TURNR)
		return DOWN;
	if (direction == RIGHT)
		return RIGHT;
	return EMPTY;
}

char threadUpdateADirectionEdge(char oldDirection, char direction, int x, int y)
{
	if (direction == LEFT && x != 0) return LEFT;
	if (direction == RIGHT && x != ROADS - 1) return RIGHT;
	if (direction == UP && y != 0) return UP;
	if (direction == DOWN && y != ROADS - 1) return DOWN;
	if (direction == LEFT && x == 0 && oldDirection == UP && y == 0) 
		return RIGHT;
	if (direction == LEFT && x == 0 && oldDirection == UP) return UP;
	if (direction == LEFT && x == 0 && oldDirection == DOWN && y == ROADS - 1) 
		return RIGHT;
	if (direction == LEFT && x == 0 && oldDirection == DOWN) 
		return DOWN;
	if (direction == LEFT && x == 0 && y == 0) return DOWN;
	if (direction == LEFT && x == 0) return UP;
	if (direction == RIGHT && x == ROADS - 1 && oldDirection == UP && y == 0) 
		return LEFT;
	if (direction == RIGHT && x == ROADS - 1 && oldDirection == UP) 
		return UP;
	if (direction == RIGHT && x == ROADS - 1 && oldDirection == DOWN && y == ROADS - 1) 
		return LEFT;
	if (direction == RIGHT && x == ROADS - 1 && oldDirection == DOWN) 
		return DOWN;
	if (direction == RIGHT && x == ROADS - 1 && y == 0) return DOWN;
	if (direction == RIGHT && x == ROADS - 1) return UP;
	if (direction == UP && y == 0 && oldDirection == LEFT && x == 0) 
		return DOWN;
	if (direction == UP && y == 0 && oldDirection == LEFT) return LEFT;
	if (direction == UP && y == 0 && oldDirection == RIGHT && x == ROADS - 1) 
		return DOWN;
	if (direction == UP && y == 0 && oldDirection == RIGHT) 
		return RIGHT;
	if (direction == UP && y == 0 && x == 0) return RIGHT;
	if (direction == UP && y == 0) return LEFT;
	if (direction == DOWN && y == ROADS - 1 && oldDirection == LEFT && x == 0) 
		return UP;
	if (direction == DOWN && y == ROADS - 1 && oldDirection == LEFT) 
		return LEFT;
	if (direction == DOWN && y == ROADS - 1 && oldDirection == RIGHT && x == ROADS - 1) 
		return UP;
	if (direction == DOWN && y == ROADS - 1 && oldDirection == RIGHT) 
		return RIGHT;
	if (direction == DOWN && y == ROADS - 1 && x == 0) return RIGHT;
	if (direction == DOWN && y == ROADS - 1) return LEFT;
		return EMPTY;
}

void threadUpdateACollision(int clientId, struct connection *connections)
{
	int i, col = 0;
	for (i = 0; i < clientId; ++i)
	{
		if (!connections[i].free && connections[i].type == CLIENTA &&
			connections[clientId].x == connections[i].prevX &&
			connections[clientId].y == connections[i].prevY &&
			connections[clientId].prevX == connections[i].x &&
			connections[clientId].prevY == connections[i].y &&
			(connections[clientId].prevY != connections[clientId].y
			|| connections[clientId].prevX != connections[clientId].x))
		{
			col = 1;
			if (!connections[i].collision)
			{
				connections[i].collision = 1;
				connections[i].score -= PENALTY;
				connections[i].x = connections[i].prevX;
				connections[i].y = connections[i].prevY;
				if(connections[i].direction == UP)
					connections[i].direction = DOWN;
				else if(connections[i].direction == DOWN)
					connections[i].direction = UP;
				else if(connections[i].direction == LEFT)
					connections[i].direction = RIGHT;
				else if(connections[i].direction == RIGHT)
					connections[i].direction = LEFT;
			}
		}
	}
	if (col && !connections[clientId].collision)
	{
		connections[clientId].collision = 1;
		connections[clientId].score -= PENALTY;
		connections[clientId].x = connections[clientId].prevX;
		connections[clientId].y = connections[clientId].prevY;
		if(connections[clientId].direction == UP)
			connections[clientId].direction = DOWN;
		else if(connections[clientId].direction == DOWN)
			connections[clientId].direction = UP;
		else if(connections[clientId].direction == LEFT)
			connections[clientId].direction = RIGHT;
		else if(connections[clientId].direction == RIGHT)
			connections[clientId].direction = LEFT;
		return;
	}
	for (i = 0; i < clientId; ++i)
	{
		if (!connections[i].free && connections[i].type == CLIENTA &&
			connections[clientId].x == connections[i].x &&
			connections[clientId].y == connections[i].y)
		{
			col = 1;
			if (!connections[i].collision)
			{
				connections[i].shouldWait = 1;
				connections[i].collision = 1;
				connections[i].score -= PENALTY;
				if(connections[i].direction == UP)
					connections[i].direction = DOWN;
				else if(connections[i].direction == DOWN)
					connections[i].direction = UP;
				else if(connections[i].direction == LEFT)
					connections[i].direction = RIGHT;
				else if(connections[i].direction == RIGHT)
					connections[i].direction = LEFT;
			}
		}
	}
	if (col && !connections[clientId].collision)
	{
		connections[clientId].collision = 1;
		connections[clientId].shouldWait = 1;
		connections[clientId].score -= PENALTY;
		if(connections[clientId].direction == UP)
			connections[clientId].direction = DOWN;
		else if(connections[clientId].direction == DOWN)
			connections[clientId].direction = UP;
		else if(connections[clientId].direction == LEFT)
			connections[clientId].direction = RIGHT;
		else if(connections[clientId].direction == RIGHT)
			connections[clientId].direction = LEFT;
		return;
	}
}

int threadUpdateA(int clientId, struct connection *connections)
{
	char direction;
	if (connections[clientId].status == 0)
	{
		fprintf(stdout, "Client: %d not responding, disconnecting\n", clientId);
		xfflush(stdout, 0);
		connections[clientId].free = 1;
		if(connections[clientId].taxiId != EMPTYN)
			connections[connections[clientId].taxiId].taxiId = EMPTYN;
		return 0;
	}
	connections[clientId].status = 0;
	direction = threadUpdateADirection(connections[clientId].direction, 
		connections[clientId].turnLR);
	connections[clientId].direction = 
		threadUpdateADirectionEdge(connections[clientId].direction, 
			direction, connections[clientId].x, connections[clientId].y);		
	connections[clientId].prevX = connections[clientId].x;
	connections[clientId].prevY = connections[clientId].y;
	if (!connections[clientId].shouldWait)
	{	
		connections[clientId].collision = 0;
		if (connections[clientId].direction == UP)
			connections[clientId].y -= 1;
		else if (connections[clientId].direction == DOWN)
			connections[clientId].y += 1;
		else if (connections[clientId].direction == LEFT)
			connections[clientId].x -= 1;
		else 
			connections[clientId].x += 1;
	}
	else
		connections[clientId].shouldWait = 0;
	connections[clientId].turnLR = TURNS;
	threadUpdateACollision(clientId, connections);
	fprintf(stdout, "Thread updated client A: %d\n", clientId);
	xfflush(stdout, 0);
	return 1;
}

int threadUpdate(int clientId, struct connection *connections)
{
	if (connections[clientId].type == CLIENTB)
		return threadUpdateB(clientId, connections);
	else
		return threadUpdateA(clientId, connections);
}

int threadSendB(int clientId, struct connection *connections, int fd)
{
	struct sockaddr_in addr = connections[clientId].addr;
	char buf[MAXBUFSEND];
	int k = 0;
	if (connections[clientId].status == 0)
	{
		fprintf(stdout, "Client B: %d is disconnected\n", clientId);
		xfflush(stdout, 0);
		return 1;
	}
	connections[clientId].status = 0;
	buf[k++] = OK;
	buf[k++] = clientId / 10 + '0';
	buf[k++] = clientId % 10 + '0';
	buf[k++] = (connections[clientId].taxiId + 1) / 10 + '0';
	buf[k++] = (connections[clientId].taxiId + 1) % 10 + '0';
	buf[k++] = (connections[clientId].x + 1) / 10 + '0';
	buf[k++] = (connections[clientId].x + 1) % 10 + '0';
	buf[k++] = (connections[clientId].y + 1) / 10 + '0';
	buf[k++] = (connections[clientId].y + 1) % 10 + '0';
	buf[k++] = (connections[clientId].prevX + 1) / 10 + '0';
	buf[k++] = (connections[clientId].prevX + 1) % 10 + '0';
	buf[k++] = (connections[clientId].prevY + 1) / 10 + '0';
	buf[k++] = (connections[clientId].prevY + 1) % 10 + '0';
	while (k < MAXBUFSEND - 1)
		buf[k++] = EMPTY;
	buf[k] = '\0';	
	fprintf(stdout, "Thread sent message to client B: %d\n", clientId);
	xfflush(stdout, 0);
	if(connections[clientId].x == connections[clientId].prevX 
		&& connections[clientId].y == connections[clientId].prevY && 
		connections[clientId].prevX != EMPTYN && connections[clientId].prevY != EMPTYN)
	{	
		buf[0] = LOOSE;
		connections[clientId].free = 1;
		if(TEMP_FAILURE_RETRY(sendto(fd, buf, MAXBUFSEND, 0, &addr, sizeof(addr))) < 0) 
			ERR("sendto:");
		return 0;
	}
	if(TEMP_FAILURE_RETRY(sendto(fd, buf, MAXBUFSEND, 0, &addr, sizeof(addr))) < 0) 
		ERR("sendto:");
	return 1;
}

int threadSendA(int clientId, struct connection *connections, int fd)
{
	struct sockaddr_in addr = connections[clientId].addr;
	char buf[MAXBUFSEND];
	int i, k = 0;
	if (connections[clientId].score <= 0)
	{
		buf[k++] = LOOSE;
		while (k < MAXBUFSEND - 1)
			buf[k++] = EMPTY;
		buf[k] = '\0';
		if(TEMP_FAILURE_RETRY(sendto(fd, buf, MAXBUFSEND, 0, &addr, sizeof(addr))) < 0) 
			ERR("sendto:");
		connections[clientId].free = 1;	
		fprintf(stdout, "Client: %d lose game, disconnecting\n", clientId);
		xfflush(stdout, 0);
		return 0;
	}
	k = 0;
	buf[k++] = OK;
	buf[k++] = clientId / 10 + '0';
	buf[k++] = clientId % 10 + '0';
	buf[k++] = (connections[clientId].taxiId + 1) / 10 + '0';
	buf[k++] = (connections[clientId].taxiId + 1) % 10 + '0';
	buf[k++] = connections[clientId].score / 100 + '0';
	buf[k++] = connections[clientId].score % 100 / 10 + '0';
	buf[k++] = connections[clientId].score % 100 % 10 + '0';
	for (i = 0; i < MAXCLIENTS; ++i)
		if (!connections[i].free && (connections[i].type == CLIENTA 
			|| (connections[clientId].taxiId == EMPTYN && 
			connections[i].taxiId == EMPTYN)))
		{
			buf[k++] = i / 10 + '0';
			buf[k++] = i % 10 + '0';
			buf[k++] = connections[i].x + '0';
			buf[k++] = connections[i].y + '0';
			if(!connections[i].collision)
				buf[k++] = connections[i].direction;
			else
				buf[k++] = COLLISION;
		}
	if (connections[clientId].taxiId != EMPTYN)
	{
		buf[k++] = connections[clientId].taxiId / 10 + '0';
		buf[k++] = connections[clientId].taxiId % 10 + '0';
		buf[k++] = connections[connections[clientId].taxiId].prevX + '0';
		buf[k++] = connections[connections[clientId].taxiId].prevY + '0';
		buf[k++] = connections[connections[clientId].taxiId].direction;
	}
	while (k < MAXBUFSEND - 1)
		buf[k++] = EMPTY;
	buf[k] = '\0';
	if(TEMP_FAILURE_RETRY(sendto(fd, buf, MAXBUFSEND, 0, &addr, sizeof(addr))) < 0) 
		ERR("sendto:");
	fprintf(stdout, "Thread sent message to client: %d\n", clientId);
	xfflush(stdout, 0);
	return 1;
}

int threadSend(int clientId, struct connection *connections, int fd)
{
	if (last_signal == SIGINT)
	{
		struct sockaddr_in addr = connections[clientId].addr;
		char buf[MAXBUFSEND];
		int k = 0;
		buf[k++] = SERVTERM;
		while (k < MAXBUFSEND - 1)
			buf[k++] = EMPTY;
		buf[k] = '\0';
		if(TEMP_FAILURE_RETRY(sendto(fd, buf, MAXBUFSEND, 0, &addr, sizeof(addr))) < 0) 
			ERR("sendto:");
		connections[clientId].free = 1;	
		fprintf(stdout, "Thread sent message to client: %d\n", clientId);
		xfflush(stdout, 0);
		return 0;
	}
	if (connections[clientId].type == CLIENTB)
		return threadSendB(clientId, connections, fd);
	else 
		return threadSendA(clientId, connections, fd);
}

void *threadClient(void *ptArg)
{
	struct threadArg arg = *((struct threadArg*)(ptArg));
	struct connection *connections = arg.connections;
	int fd = *arg.fd;
	int clientId = arg.assignedClient;
	int *cond_var = arg.cond_var;
	int ifcontinue = 1;
	pthread_cond_t *cond = arg.cond;
	pthread_mutex_t *mutex = arg.mutex;
	sigset_t mask, oldmask;
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);
	sigaddset (&mask, SIGINT);
	pthread_sigmask(SIG_BLOCK, &mask, &oldmask);
	while(last_signal != SIGINT && ifcontinue)
	{
		if(pthread_mutex_lock(mutex) != 0)
			ERR("Mutex Lock");
		while(*cond_var != clientId)
			if(pthread_cond_wait(cond, mutex) != 0)
				ERR("pthread_cond_wait");
		if(last_signal != SIGINT)
			ifcontinue = threadUpdate(clientId, connections);
		*cond_var = MAINPT;
		if (pthread_cond_broadcast(cond) != 0)
			ERR("pthread_cond_broadcast");
		if(pthread_mutex_unlock(mutex) != 0)
			ERR("Mutex UnLock");
		if (ifcontinue)
		{
			if(pthread_mutex_lock(mutex) != 0)
				ERR("Mutex Lock");
			while(*cond_var != clientId)
				if(pthread_cond_wait(cond, mutex) != 0)
					ERR("pthread_cond_wait");
			ifcontinue = threadSend(clientId, connections, fd);
			*cond_var = MAINPT;
			if (pthread_cond_broadcast(cond) != 0)
				ERR("pthread_cond_broadcast");
			if(pthread_mutex_unlock(mutex) != 0)
				ERR("Mutex UnLock");
		}
	}
	fprintf(stdout, "Thread for client: %d will terminate\n", clientId);
	xfflush(stdout, 0);
	pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
	pthread_exit(NULL);
}

void wakeUpThreads(struct connection *connections, 
	pthread_cond_t *cond, pthread_mutex_t *mutex, int *cond_var)
{
	int i;	
	fprintf(stdout, "Wake up threads\n");
	xfflush(stdout, 0);
	for(i = 0; i < MAXCLIENTS; ++i)
	{
		if(pthread_mutex_lock(mutex) != 0)
			ERR("Mutex Lock");
		while(*cond_var != MAINPT)
			if(pthread_cond_wait(cond, mutex) != 0)
				ERR("pthread_cond_wait");
		if(!connections[i].free && connections[i].type == CLIENTA)
		{			
			*cond_var = i;
			if (pthread_cond_broadcast(cond) != 0)
				ERR("pthread_cond_broadcast");
		}	
		if(pthread_mutex_unlock(mutex) != 0)
			ERR("Mutex UnLock");
	}
	for(i = 0; i < MAXCLIENTS; ++i)
	{
		if(pthread_mutex_lock(mutex) != 0)
			ERR("Mutex Lock");
		while(*cond_var != MAINPT)
			if(pthread_cond_wait(cond, mutex) != 0)
				ERR("pthread_cond_wait");
		if(!connections[i].free && connections[i].type == CLIENTB)
		{		
			*cond_var = i;
			if (pthread_cond_broadcast(cond) != 0)
				ERR("pthread_cond_broadcast");
		}
		if(pthread_mutex_unlock(mutex) != 0)
			ERR("Mutex UnLock");
	}
	for(i = 0; i < MAXCLIENTS; ++i)	
	{
		if(pthread_mutex_lock(mutex) != 0)
			ERR("Mutex Lock");
		while(*cond_var != MAINPT)
			if(pthread_cond_wait(cond, mutex) != 0)
				ERR("pthread_cond_wait");
		if(!connections[i].free)
		{	
			*cond_var = i;
			if (pthread_cond_broadcast(cond) != 0)
				ERR("pthread_cond_broadcast");

		}	
		if(pthread_mutex_unlock(mutex) != 0)
			ERR("Mutex UnLock");
	}
	if(pthread_mutex_lock(mutex) != 0)
		ERR("Mutex Lock");
	while(*cond_var != MAINPT)
		if(pthread_cond_wait(cond, mutex) != 0)
			ERR("pthread_cond_wait");
	if(pthread_mutex_unlock(mutex) != 0)
		ERR("Mutex UnLock");
	if(last_signal != SIGINT)
		last_signal = 0;
	alarm(SLEEPTIME);
}

void addNewClient(struct sockaddr_in addr, struct threadArg *ptArg, char type)
{
	pthread_t thread;
	pthread_attr_t attr;
	int i = checkIndex(addr, ptArg->connections), success = 1;
	if (i >= 0)
	{
		SendResponse(*(ptArg->fd), addr);
		fprintf(stdout, "Error, addNewClient, client %d already exists\n", i);
		xfflush(stdout, 0);
		return;
	}	
	if(pthread_mutex_lock(ptArg->mutex) != 0)
		ERR("Mutex Lock");
	while(*(ptArg->cond_var) != MAINPT)
		if(pthread_cond_wait(ptArg->cond, ptArg->mutex) != 0)
			ERR("pthread_cond_wait");
	i = findFreeIndex(addr, ptArg->connections);
	if(i < 0 || i >= MAXCLIENTS)
	{
		SendResponse(*(ptArg->fd), addr);
		fprintf(stdout, "Error, addNewClient, new client cannot be added, no free place\n");
		xfflush(stdout, 0);
		if(pthread_mutex_unlock(ptArg->mutex) != 0)
			ERR("Mutex UnLock");
		return;
	}
	ptArg->connections[i].status = 1;
	ptArg->connections[i].score = SCORE;
	ptArg->connections[i].turnLR = TURNS;
	ptArg->connections[i].prevX = EMPTYN;
	ptArg->connections[i].prevY = EMPTYN;
	ptArg->connections[i].collision = 0;
	ptArg->connections[i].taxiId = EMPTYN;		
	ptArg->connections[i].type = CLIENTB;
	ptArg->connections[i].x = EMPTYN;
	ptArg->connections[i].y = EMPTYN;
	ptArg->connections[i].direction = TASK;
	ptArg->connections[i].shouldWait = 0;
	success = 1;
	if (type == CLIENTA)
	{
		ptArg->connections[i].type = CLIENTA;
		success = randPosition(i, ptArg->connections);
	}	
	if(!success)
	{	
		SendResponse(*(ptArg->fd), addr);
		ptArg->connections[i].free = 1;
		fprintf(stdout, "Error, new client cannot be added\n");
		xfflush(stdout, 0);
		if(pthread_mutex_unlock(ptArg->mutex) != 0)
			ERR("Mutex UnLock");
		return;
	}	
	if(pthread_mutex_unlock(ptArg->mutex) != 0)
		ERR("Mutex UnLock");
	if(pthread_attr_init(&attr) != 0) 
		ERR("pthread_attr_init:");
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	ptArg->assignedClient = i;
	if(pthread_create(&thread, &attr, &threadClient, (void*)ptArg) != 0) 
		ERR("pthread_create:");
	pthread_attr_destroy(&attr);
	fprintf(stdout, "Client: %d created\n", i);
	xfflush(stdout, 0);
}

void updateStatus(struct sockaddr_in addr, struct connection *connections, 
	pthread_cond_t *cond, pthread_mutex_t *mutex, int *cond_var)
{
	int i = checkIndex(addr, connections);
	if (i < 0)
	{
		fprintf(stdout, "Error, update status, client not found\n");
		xfflush(stdout, 0);
		return;
	}
	if(pthread_mutex_lock(mutex) != 0)
		ERR("Mutex Lock");
	while(*cond_var != MAINPT)
		if(pthread_cond_wait(cond, mutex) != 0)
			ERR("pthread_cond_wait");
	connections[i].status++;
	if(pthread_mutex_unlock(mutex) != 0)
		ERR("Mutex UnLock");
	fprintf(stdout, "Client A/B: %d status updated\n", i);
	xfflush(stdout, 0);
}

void turnLeftRight(struct sockaddr_in addr,	struct connection *connections, 
	pthread_cond_t *cond, pthread_mutex_t *mutex, int *cond_var, char lr)
{
	int i = checkIndex(addr, connections);
	if (i < 0)
	{
		fprintf(stdout, "Error, turnLeftRight, client not found\n");
		xfflush(stdout, 0);
		return;
	}
	if (connections[i].type != CLIENTA)
	{
		fprintf(stdout, "Error, turnLeftRight, client B cannot turn\n");
		xfflush(stdout, 0);
		return;
	}
	if(pthread_mutex_lock(mutex) != 0)
		ERR("Mutex Lock");
	while(*cond_var != MAINPT)
		if(pthread_cond_wait(cond, mutex) != 0)
			ERR("pthread_cond_wait");
	connections[i].turnLR = lr;
	if(pthread_mutex_unlock(mutex) != 0)
		ERR("Mutex UnLock");
	fprintf(stdout, "Client A: %d turned: %c\n", i, lr);
	xfflush(stdout, 0);
}

void setTaskXY(struct sockaddr_in addr, struct connection *connections, 
	pthread_cond_t *cond, pthread_mutex_t *mutex, int *cond_var, char* buf)
{
	int i = checkIndex(addr, connections), 
		startX = EMPTYN, startY = EMPTYN, endX = EMPTYN, endY = EMPTYN;
	if (i < 0)
	{
		fprintf(stdout, "Error, setTaskXY, client not found\n");
		xfflush(stdout, 0);
		return;
	}
	if (connections[i].type != CLIENTB)
	{
		fprintf(stdout, "Error, setTaskXY, client A cannot set position\n");
		xfflush(stdout, 0);
		return;
	}	
	if(connections[i].x != EMPTYN && connections[i].y != EMPTYN && connections[i].prevX != EMPTYN && connections[i].prevY != EMPTYN)
	{
		fprintf(stdout, "Error, setTaskXY, coordinates already updated\n");
		xfflush(stdout, 0);
		return;	
	}
	startX = (buf[0] - '0') * 10 + (buf[1] - '0');
	startY = (buf[3] - '0') * 10 + (buf[4] - '0');
	endX = (buf[6] - '0') * 10 + (buf[7] - '0');
	endY = (buf[9] - '0') * 10 + (buf[10] - '0');
	if(startX < 0 || startX >= ROADS || startY < 0 || startY >= ROADS ||
		endX < 0 || endX >= ROADS || endY < 0 || endY >= ROADS || 
		(startX == endX && startY == endY))
	{
		fprintf(stdout, "Error, setTaskXY, incorrect coordinates\n");
		xfflush(stdout, 0);
		return;	
	}
	if(pthread_mutex_lock(mutex) != 0)
		ERR("Mutex Lock");
	while(*cond_var != MAINPT)
		if(pthread_cond_wait(cond, mutex) != 0)
			ERR("pthread_cond_wait");
	connections[i].x = startX;
	connections[i].y = startY;
	connections[i].prevX = endX;
	connections[i].prevY = endY;
	if(pthread_mutex_unlock(mutex) != 0)
		ERR("Mutex UnLock");
	fprintf(stdout, "Client B: %d coordiantes updated: %d, %d, %d, %d\n", i, startX, startY, endX, endY);
	xfflush(stdout, 0);
}

void doServer(int fd)
{
	pthread_cond_t *cond = xmalloc(sizeof(pthread_cond_t));
	pthread_mutex_t *mutex = xmalloc(sizeof(pthread_mutex_t));
	int *cond_var = xmalloc(sizeof(int));
	struct connection* connections = xmalloc(MAXCLIENTS * sizeof(struct connection));	
	struct threadArg *ptArg = xmalloc(sizeof(struct threadArg));
	struct sockaddr_in addr;
	char buf[MAXBUFRECV];
	int i;
	socklen_t size = sizeof(addr);
	*cond_var = MAINPT;
	ptArg->connections = connections;
	ptArg->fd = xmalloc(sizeof(int));
	*(ptArg->fd) = fd;
	ptArg->mutex = mutex;
	ptArg->cond = cond;
	ptArg->cond_var = cond_var;
	if(pthread_mutex_init(mutex, NULL))
		ERR("init_mutex");
	if(pthread_cond_init(cond, NULL))
		ERR("init_cond");
	if(sethandler(signal_handler, SIGALRM)) 
		ERR("Seting SIGALRM");
	if(pthread_mutex_lock(mutex) != 0)
		ERR("Mutex Lock");
	while(*cond_var != MAINPT)
		if(pthread_cond_wait(cond, mutex) != 0)
			ERR("pthread_cond_wait");
	for(i = 0; i < MAXCLIENTS; i++)
		connections[i].free = 1;	
	if(pthread_mutex_unlock(mutex) != 0)
		ERR("Mutex UnLock");
	alarm(SLEEPTIME);
	while(last_signal != SIGINT)
	{
		while(0 == last_signal && 
			recvfrom(fd, buf, MAXBUFRECV, 0, &addr, &size) < 0)
		{
			if(EINTR != errno)
				ERR("recvfrom:");
			if(0 != last_signal) 
				break;
		}
		if (0 != last_signal)
			wakeUpThreads(connections, cond, mutex, cond_var);
		else if (strcmp(buf, HELLOAMSG) == 0)
			addNewClient(addr, ptArg, CLIENTA);
		else if (strcmp(buf, HELLOBMSG) == 0)
			addNewClient(addr, ptArg, CLIENTB);
		else if (strcmp(buf, STATUSMSG) == 0)
			updateStatus(addr, connections, cond, mutex, cond_var);
		else if (strcmp(buf, TURNLMSG) == 0)
			turnLeftRight(addr, connections, cond, mutex, cond_var, TURNL);
		else if (strcmp(buf, TURNRMSG) == 0)
			turnLeftRight(addr, connections, cond, mutex, cond_var, TURNR);
		else
			setTaskXY(addr, connections, cond, mutex, cond_var, buf);
	}
	free(cond);
	free(cond_var);
	free(connections);
	free(mutex);
	free(ptArg->fd);
	free(ptArg);
}

int main(int argc, char** argv) 
{
	int fd;
	if(argc != ARGS) 
	{
		usage(argv[0]);
		return EXIT_FAILURE;
	}	
	if(sethandler(SIG_IGN, SIGPIPE)) 
		ERR("Seting SIGPIPE:");
	if(sethandler(signal_handler, SIGINT)) 
		ERR("Seting SIGINT:");
	fd = bind_inet_socket(atoi(argv[1]), SOCK_DGRAM);
	doServer(fd);
	if(TEMP_FAILURE_RETRY(close(fd)) < 0)
		ERR("close");
	fprintf(stdout, "Server has terminated.\n");
	xfflush(stdout, 0);
	pthread_exit(EXIT_SUCCESS);
}
