/*
 * ftp_impl.c
 *
 *  Created on: Nov 10, 2016
 *      Author: janne
 */

#include "../inc/ftp_impl.h"
#include "../inc/socket_wrapper_impl.h"
#include <stdio.h>

void run_ftp()
{
	printf("FTP server starting\n");
	struct socket_server server;
	init_server_socket(&server);

	int serverSocketFd = server.get_server_socket_fd("3370");

	int clientSocketFd = server.wait_for_connection(serverSocketFd);

	server.conn.disconnect(clientSocketFd);
	server.conn.disconnect(serverSocketFd);
}
