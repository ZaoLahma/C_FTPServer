/*
 * ftp_impl.c
 *
 *  Created on: Nov 10, 2016
 *      Author: janne
 */

#include "../inc/ftp_impl.h"
#include "../inc/socket_wrapper_impl.h"
#include "../inc/thread_starter_impl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct client_conn
{
	int controlFd;
	int dataFd;
	struct socket_common* connection;
};

static void handshake(struct client_conn* clientConn)
{
	clientConn->connection->send(clientConn->controlFd, "220 OK.\r\n", 9);

	char receiveBuf[100];

	clientConn->connection->receive(clientConn->controlFd, receiveBuf, 100);

	printf("receiveBuf: %s (%d)\n", receiveBuf, strlen(receiveBuf));


}

static void* client_conn_main(void* arg)
{
	struct client_conn* clientConn = (struct client_conn*)arg;

	handshake(clientConn);

	clientConn->connection->disconnect(clientConn->controlFd);

	return 0;
}

void run_ftp()
{
	printf("FTP server starting\n");
	struct socket_server server;
	init_server_socket(&server);

	int serverSocketFd = server.get_server_socket_fd("3370");
	int clientSocketFd = -1;

	struct ThreadStarter thread;
	init_thread_starter(&thread, POOL);

	while(1)
	{
		clientSocketFd = server.wait_for_connection(serverSocketFd);

		struct client_conn* client = (struct client_conn*)malloc(sizeof(struct client_conn));
		client->controlFd = clientSocketFd;
		client->connection = &server.conn;

		thread.execute_function(&client_conn_main, client);
	}

	server.conn.disconnect(clientSocketFd);
	server.conn.disconnect(serverSocketFd);
}
