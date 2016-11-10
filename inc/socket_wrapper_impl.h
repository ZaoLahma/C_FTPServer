/*
 * socket_wrapper_impl.h
 *
 *  Created on: Nov 9, 2016
 *      Author: janne
 */

#ifndef INC_SOCKET_WRAPPER_IMPL_H_
#define INC_SOCKET_WRAPPER_IMPL_H_

#include "socket_wrapper.h"

void init_client_socket(struct socket_client* socket);
void init_server_socket(struct socket_server* socket);


#endif /* INC_SOCKET_WRAPPER_IMPL_H_ */
