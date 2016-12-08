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
#include <pthread.h>
#include "../inc/thread_starter.h"
#include "../inc/thread_starter_impl.h"
#include "../inc/socket_wrapper_impl.h"
#include "../inc/ftp_impl.h"

#define EXPECT(this, that) \
if(this != that) \
{\
	printf("\n---- TEST FAILED ----\n");\
	printf("File: %s, line: %d\n", __FILE__, __LINE__);\
	printf(#this " (0x%x) != " #that " (0x%x)\n", \
		  (unsigned int)this,\
		  (unsigned int)that);\
	abort();\
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
	EXPECT(4, noOfBytesReceived);
	EXPECT(0, strcmp(recv, "hej"));

	char to_send[6] = "hello";
	int noOfBytesSent = client.conn.send(serverFd, to_send, 6);
	EXPECT(6, noOfBytesSent);

	client.conn.disconnect(serverFd);
	printf("After connect\n");

	return 0;
}

#define TEST_PORT_NO "45972"
static pthread_mutex_t portMutex;
static pthread_cond_t portCond;
void* ftp_test_port_func(void* arg)
{
	struct socket_server server;
	init_server_socket(&server);

	int serverFd = server.get_server_socket_fd(TEST_PORT_NO);

	int* clientFd = (int*)arg;

    pthread_mutex_lock(&portMutex);
    *clientFd = -1;
    while(*clientFd == -1)
    {
        printf("Test case waiting for connection\n");
        *clientFd = server.wait_for_connection(serverFd);
	}
	pthread_cond_signal(&portCond);
	pthread_mutex_unlock(&portMutex);

	printf("Test case received connection. clientFd: %d\n", *clientFd);

	return 0;
}

static int run_list_command(int* serverFd, int* clientFd, socket_client* client)
{
	client->conn.send(*serverFd, "LIST\r\n", 6);

	char receiveBuf[100] = "";
	int noOfBytesReceived = client->conn.receive(*serverFd, receiveBuf, 100);
	receiveBuf[noOfBytesReceived] = '\0';

	EXPECT(0, strcmp("150 LIST executed ok, data follows\r\n", receiveBuf));

	while((noOfBytesReceived = client->conn.receive(*clientFd, receiveBuf, 100)) != 0)
	{
		receiveBuf[noOfBytesReceived] = '\0';
		printf("receiveBuf: %s\n", receiveBuf);
	}

	client->conn.disconnect(*clientFd);
	*clientFd = 0xffffffff;

	noOfBytesReceived = client->conn.receive(*serverFd, receiveBuf, 100);

	receiveBuf[noOfBytesReceived] = '\0';
	printf("receiveBuf: %s\n", receiveBuf);

	EXPECT(0, strcmp("226 LIST data send finished\r\n", receiveBuf));

	return 0;
}

void* ftp_test_func(void* arg)
{
	printf("arg: %p\n", arg);

	int* running = (int*)arg;

	int noOfBytesReceived = 0;

	char receiveBuf[100] = "";

	struct socket_client client;
	init_client_socket(&client);

	sleep(1);

	int serverFd = client.connect("127.0.0.1", "3370");

	noOfBytesReceived = client.conn.receive(serverFd, receiveBuf, 100);
	receiveBuf[noOfBytesReceived] = '\0';

	EXPECT(0, strcmp("220 OK\r\n", receiveBuf));

	client.conn.send(serverFd, "USER janne\r\n", 12);

	noOfBytesReceived = client.conn.receive(serverFd, receiveBuf, 100);
	receiveBuf[noOfBytesReceived] = '\0';

	EXPECT(0, strcmp("330 OK, send password\r\n", receiveBuf));

	client.conn.send(serverFd, "PASS hihello\r\n", 14);

	noOfBytesReceived = client.conn.receive(serverFd, receiveBuf, 100);
	receiveBuf[noOfBytesReceived] = '\0';

	EXPECT(0, strcmp("230 OK, user logged in\r\n", receiveBuf));

	struct ThreadStarter threadStarter;
	init_thread_starter(&threadStarter, DETACHED);

	int* clientFd = (int*)malloc(sizeof(int));
	*clientFd = 0xffffffff;

	threadStarter.execute_function(&ftp_test_port_func, clientFd);

    pthread_mutex_lock(&portMutex);
    sleep(1);

	client.conn.send(serverFd, "PORT 127,0,0,1,179,148\r\n", 24);

    pthread_cond_wait(&portCond, &portMutex); //Wait for ftp_test_port_func to have received a connection
    pthread_mutex_unlock(&portMutex);

	noOfBytesReceived = client.conn.receive(serverFd, receiveBuf, 100);
	receiveBuf[noOfBytesReceived] = '\0';

	//------
	EXPECT(0, strcmp("200 PORT command successful\r\n", receiveBuf));

	EXPECT(1, (0xffffffff != (unsigned int)*clientFd));

	EXPECT(0, run_list_command(&serverFd, clientFd, &client));

	//------
	client.conn.send(serverFd, "PASV\r\n", 6);

	noOfBytesReceived = client.conn.receive(serverFd, receiveBuf, 100);
	receiveBuf[noOfBytesReceived] = '\0';
	EXPECT(0, strcmp("227 PASV (addr:127,0,0,1,10,10)\r\n", receiveBuf));

	*clientFd = client.connect("127.0.0.1", "2570");

	EXPECT(0, run_list_command(&serverFd, clientFd, &client));

	//------
	client.conn.send(serverFd, "PASV\r\n", 6);

	noOfBytesReceived = client.conn.receive(serverFd, receiveBuf, 100);
	receiveBuf[noOfBytesReceived] = '\0';
	EXPECT(0, strcmp("227 PASV (addr:127,0,0,1,10,10)\r\n", receiveBuf));

	printf("EXPECTING TIMEOUT IN 2.5 SECONDS\n");

	//------
	client.conn.send(serverFd, "CWD test\r\n", 10);

	noOfBytesReceived = client.conn.receive(serverFd, receiveBuf, 100);
	receiveBuf[noOfBytesReceived] = '\0';
	EXPECT(0, strcmp("250 CWD OK\r\n", receiveBuf));

	client.conn.send(serverFd, "CWD ..\r\n", 10);

	noOfBytesReceived = client.conn.receive(serverFd, receiveBuf, 100);
	receiveBuf[noOfBytesReceived] = '\0';
	EXPECT(0, strcmp("250 CWD OK\r\n", receiveBuf));

	//------
	client.conn.send(serverFd, "QUIT\r\n", 6);

	noOfBytesReceived = client.conn.receive(serverFd, receiveBuf, 100);
	receiveBuf[noOfBytesReceived] = '\0';

	EXPECT(0, strcmp("221 Bye Bye\r\n", receiveBuf));

	*running = 0;
	serverFd = client.connect("127.0.0.1", "3370");

	return 0;
}

void test_framework()
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

	printf("----- SOCKET TEST----- \n");

	struct socket_client client;
	init_client_socket(&client);

	struct socket_server server;
	init_server_socket(&server);

	int socketFd = server.get_server_socket_fd("3370");

	printf("socketFd: %d\n", socketFd);

	threadStarter.execute_function(&connect_func, 0);

	int clientFd = server.wait_for_connection(socketFd);

	char to_send[4] = "hej";
	int noOfBytesSent = server.conn.send(clientFd, to_send, 4);
	EXPECT(4, noOfBytesSent);

	char recv[6];
	int noOfBytesReceived = server.conn.receive(clientFd, recv, 6);
	EXPECT(6, noOfBytesReceived);
	EXPECT(0, strcmp(recv, "hello"));

	server.conn.disconnect(clientFd);
	server.conn.disconnect(socketFd);
}

void test_ftp()
{
    pthread_mutex_init(&portMutex, 0);
    pthread_cond_init(&portCond, 0);

	struct ThreadStarter threadStarter;
	init_thread_starter(&threadStarter, DETACHED);

	int* running = (int*)malloc(sizeof(int));
	*running = 1;

	threadStarter.execute_function(&ftp_test_func, running);

	unsigned char addr[4] = {127, 0, 0, 1};

	run_ftp(running, addr, "3370");

	free(running);
}

int main(void)
{
	test_framework();
	test_ftp();

	printf("\n---- TEST SUCCEEDED ----\n");
	return 0;
}
