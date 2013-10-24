/*
 * sock_op.h
 *
 *  Created on: Sep 28, 2013
 *      Author: csci551
 */

#ifndef SOCK_OP_H_
#define SOCK_OP_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

int create_tcp_socket();

void populate_sockaddr_in(struct sockaddr_in *sk_address,const char* address,int port);

int bind_address(int tcp_sock_fd, struct sockaddr_in tcp_sender);

#endif /* SOCK_OP_H_ */
