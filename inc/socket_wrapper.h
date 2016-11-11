/*
 * socket_wrapper.h
 *
 *  Created on: Nov 1, 2016
 *      Author: janne
 */

#ifndef INC_SOCKET_WRAPPER_H_
#define INC_SOCKET_WRAPPER_H_

struct socket_common
{
	void (*disconnect)(int socketFd);
	int (*send)(int fileDesc, void* data, int size);
	int (*receive)(int fileDesc, void* data, int max_size);
};

struct socket_client
{
	int (*connect)(char* address, char* portNo);
	struct socket_common conn;
};

struct socket_server
{
	int (*get_server_socket_fd)(char* portNo);
	int (*wait_for_connection)();
	struct socket_common conn;
};

#endif /* INC_SOCKET_WRAPPER_H_ */
