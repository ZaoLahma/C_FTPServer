/*
 * socket_wrapper_impl.c
 *
 *  Created on: Nov 9, 2016
 *      Author: janne
 */

#include "../inc/socket_wrapper_impl.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>

//----- Common -----
static void disconnect(int socketFd)
{
	close(socketFd);
}

static int socket_send(int fileDesc, void* data, int size)
{
	int noOfBytesSent = 0;

	noOfBytesSent = send(fileDesc, data, size, 0);

	return noOfBytesSent;
}

static int socket_receive(int fileDesc, void* data, int max_size)
{
	int noOfBytesReceived = 0;

	noOfBytesReceived = recv(fileDesc, data, max_size, 0);

	return noOfBytesReceived;
}

//----- Server -----
static int get_server_socket_fd(char* portNo)
{
    int sockfd;
    struct addrinfo hints;
	struct addrinfo* servinfo;
    struct addrinfo* p;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(0, portNo, &hints, &servinfo)) != 0) {
        return -1;
    }


    for(p = servinfo; p != 0; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &yes,
                sizeof(int)) == -1) {
            return -1;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == 0)  {
        return -1;
    }

    if (listen(sockfd, 10) == -1) {
        return -1;
    }

    return sockfd;
}

static int wait_for_connection(int socketFd)
{

    struct timeval tv;
    fd_set acceptFds;

    // Wait at most 2.5 seconds for a client to connect
    tv.tv_sec = 2;
    tv.tv_usec = 500000;

	struct sockaddr_storage their_addr;
    socklen_t sin_size;

	sin_size = sizeof their_addr;

    FD_ZERO(&acceptFds);
    FD_SET(socketFd, &acceptFds);

    select(socketFd + 1, &acceptFds, 0, 0, &tv);

    if (FD_ISSET(socketFd, &acceptFds))
    {
    	return accept(socketFd, (struct sockaddr *)&their_addr, &sin_size);

    }

    return -1;
}

void init_server_socket(struct socket_server* socket)
{
	socket->get_server_socket_fd = &get_server_socket_fd;
	socket->wait_for_connection = &wait_for_connection;
	socket->conn.disconnect = &disconnect;
	socket->conn.send = &socket_send;
	socket->conn.receive = &socket_receive;
}

//----- Client -----
static int connect_to_server(char* address, char* portNo)
{
    int sockfd;

    struct addrinfo hints;
	struct addrinfo* servinfo;
	struct addrinfo* p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(address, portNo, &hints, &servinfo) != 0) {
    	printf("getddrinfo\n");
        return -1;
    }

    for(p = servinfo; p != 0; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == 0) {
    	printf("p==0, errno: %d\n", errno);
        return -1;
    }

    freeaddrinfo(servinfo);

    return sockfd;
}

void init_client_socket(struct socket_client* socket)
{
	socket->connect = &connect_to_server;
	socket->conn.disconnect = &disconnect;
	socket->conn.send = &socket_send;
	socket->conn.receive = &socket_receive;
}

