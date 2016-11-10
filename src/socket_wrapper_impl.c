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

void disconnect(int socketFd)
{
	close(socketFd);
}

int get_server_socket_fd(char* portNo)
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

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
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

int connect_to_server(char* address, char* portNo)
{
    int sockfd;

    struct addrinfo hints;
	struct addrinfo* servinfo;
	struct addrinfo* p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(address, portNo, &hints, &servinfo)) != 0) {
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
        return -1;
    }

    freeaddrinfo(servinfo);

    return sockfd;
}

int wait_for_connection(int socketFd)
{
	struct sockaddr_storage their_addr;
    socklen_t sin_size;

	sin_size = sizeof their_addr;
	int new_fd = accept(socketFd, (struct sockaddr *)&their_addr, &sin_size);

    return new_fd;
}

void init_client_socket(struct socket_client* socket)
{
	socket->connect = &connect_to_server;
	socket->conn.disconnect = &disconnect;
}

void init_server_socket(struct socket_server* socket)
{
	socket->get_server_socket_fd = &get_server_socket_fd;
	socket->wait_for_connection = &wait_for_connection;
	socket->conn.disconnect = &disconnect;
}
