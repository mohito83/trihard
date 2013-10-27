/*
 * sock_op.c
 *
 *  Created on: Sep 28, 2013
 *      Author: csci551
 */

#include "sock_op.h"

//Re-used code from my CSCI558L FTP assignment -- STARTS
int create_tcp_socket() {
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		perror("Error while creating TCP socket.");
	return sockfd;
}

/**     Populate struct sockaddr_in structure. This piece of code is common to both UDP and TCP
 *       @sk_address: The data structure that will be populated.
 *       @address: The remote address. For localhost send this as NULL. It will bind using INADDR_ANY.
 */
void populate_sockaddr_in(struct sockaddr_in *sk_address, const char* address,
		int port) {
	struct hostent *remote_entry;

	memset(sk_address,0, sizeof(struct sockaddr_in));
	sk_address->sin_family = AF_INET;
	sk_address->sin_port = htons(port);

	//For sender. Get address of the remote host and populate the data structure.
	if (address != NULL) {
		remote_entry = gethostbyname(address);
		bcopy((char *) remote_entry->h_addr,
		(char *)&sk_address->sin_addr.s_addr,
		remote_entry->h_length);
	}
	//For receiver bind to a socket on the local host.
	else
	{
		sk_address->sin_addr.s_addr = INADDR_ANY;
	}

}

/**
 * This function will bind the socket to TCP address
 */
int bind_address(int tcp_sock_fd, struct sockaddr_in tcp_sender) {
	int r_code = bind(tcp_sock_fd, (struct sockaddr *) &tcp_sender, sizeof(tcp_sender));

	if(r_code<0)
		close(tcp_sock_fd);

	return r_code;
}

//USED CODE ENDS HERE--

int create_udp_socket(){
	int sockfd;
		if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
			perror("Error while creating UDP socket.");
		return sockfd;
}
