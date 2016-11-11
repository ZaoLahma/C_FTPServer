/*
 * test.c
 *
 *  Created on: Oct 16, 2016
 *      Author: janne
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../inc/thread_starter.h"
#include "../inc/thread_starter_impl.h"
#include "../inc/socket_wrapper_impl.h"

#define EXPECT(this, that) \
if(this != that) \
{\
	printf(#this " (0x%x) != " #that " (0x%x)\n", \
		  (unsigned int)this, \
		  (unsigned int)that);\
}\
else\
{\
	printf(#this " == " #that " validated OK\n");\
}

void* thread_func_1(void* arg)
{
	printf("This is executed in a thread\n");

	unsigned int* testInt = (unsigned int*)arg;
	EXPECT(1, *testInt);

	return 0;
}

void* thread_func_2(void* arg)
{
	unsigned int* testInt = (unsigned int*)arg;
	EXPECT(2, *testInt);

	return 0;
}

void* connect_func(void* arg)
{
	printf("%p\n", arg);
	struct socket_client client;
	init_client_socket(&client);
	sleep(1);

	printf("Connecting\n");
	int serverFd = client.connect("127.0.0.1", "3370");

	char recv[4];
	int noOfBytesReceived = client.conn.receive(serverFd, recv, 4);
	EXPECT(3, noOfBytesReceived);
	EXPECT(0, strcmp(recv, "hej"));

	char to_send[6] = {'h', 'e', 'l', 'l', 'o', '\0'};
	int noOfBytesSent = client.conn.send(serverFd, to_send, 6);
	EXPECT(6, noOfBytesSent);

	client.conn.disconnect(serverFd);
	printf("After connect\n");

	return 0;
}

int main(void)
{
	printf("----- DETACHED TEST----- \n");

	struct ThreadStarter threadStarter;
	init_thread_starter(&threadStarter, DETACHED);

	unsigned int* testInt = (unsigned int*)malloc(sizeof(unsigned int));
	*testInt = 1;

	unsigned int* testInt_2 = (unsigned int*)malloc(sizeof(unsigned int));
	*testInt_2 = 2;
	threadStarter.execute_function(&thread_func_1, testInt);
	threadStarter.execute_function(&thread_func_2, testInt_2);

	sleep(1);

	printf("----- POOL TEST----- \n");

	init_thread_starter(&threadStarter, POOL);

	threadStarter.execute_function(&thread_func_1, testInt);
	threadStarter.execute_function(&thread_func_2, testInt_2);
	threadStarter.execute_function(&thread_func_2, testInt_2);
	threadStarter.execute_function(&thread_func_1, testInt);
	threadStarter.execute_function(&thread_func_2, testInt_2);
	threadStarter.execute_function(&thread_func_1, testInt);
	threadStarter.execute_function(&thread_func_1, testInt);

	sleep(1);

	free(testInt);

	struct socket_client client;
	init_client_socket(&client);

	struct socket_server server;
	init_server_socket(&server);

	int socketFd = server.get_server_socket_fd("3370");

	printf("socketFd: %d\n", socketFd);

	threadStarter.execute_function(&connect_func, 0);

	int clientFd = server.wait_for_connection(socketFd);

	char to_send[4] = {'h', 'e', 'j', '\0'};
	int noOfBytesSent = server.conn.send(clientFd, to_send, 3);
	EXPECT(3, noOfBytesSent);

	char recv[6];
	int noOfBytesReceived = server.conn.receive(clientFd, recv, 6);
	EXPECT(6, noOfBytesReceived);
	EXPECT(0, strcmp(recv, "hello"));

	server.conn.disconnect(clientFd);
	server.conn.disconnect(socketFd);

	return 0;
}
