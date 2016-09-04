C_FLAGS = -Wall -Werror -pedantic -g
L_FLAGS = -lm -lrt -lpthread

SERVER = server
SERVER_FILES = server.o common.o

CLIENT = client
CLIENT_FILES = client.o client_common.o common.o

CLIENT_TASK = client_task
CLIENT_TASK_FILES = client_task.o client_common.o common.o

.PHONY: all clean

all: ${SERVER} ${CLIENT} ${CLIENT_TASK}

${SERVER}: ${SERVER_FILES}
	gcc -o ${SERVER} ${SERVER_FILES} ${L_FLAGS}

${CLIENT}: ${CLIENT_FILES}
	gcc -o ${CLIENT} ${CLIENT_FILES} ${L_FLAGS}
	
${CLIENT_TASK}: ${CLIENT_TASK_FILES}
	gcc -o ${CLIENT_TASK} ${CLIENT_TASK_FILES} ${L_FLAGS}

server.o: server.c
	gcc -c server.c -o server.o ${C_FLAGS}
	
client.o: client.c 
	gcc -c client.c -o client.o ${C_FLAGS}

client_task.o: client_task.c 
	gcc -c client_task.c -o client_task.o ${C_FLAGS}	
	
common.o: common.c 
	gcc -c common.c -o common.o ${C_FLAGS}
	
client_common.o: client_common.c 
	gcc -c client_common.c -o client_common.o ${C_FLAGS}
	
clean:
	-rm -f ${CLIENT} ${CLIENT_FILES} ${CLIENT_TASK} ${CLIENT_TASK_FILES} \
		${SERVER} ${SERVER_FILES}
