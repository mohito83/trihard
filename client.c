/*
 * client.c
 *
 *  Created on: Oct 24, 2013
 *      Author: csci551
 */

#include "client.h"
#include "sha1.h"
#include "file_io_op.h"

char client_output_filename[100], client_name[80], second_name[80], s[SEGSIZE],
		buffer[MAXSIZE];
char *client_shm;
int stage, status, client_shmid, tcp_client_sock_fd, udp_sock_fd,
		local_udp_port = 0, client_1_port_no;
struct sockaddr_in client_tcp_server, udp_host_sock_addr;
key_t client_key;
fd_set readfds;
long nonce;
FILE *client_output_fd;
unsigned int self_triad_id, predecessor_triad_id, successor_triad_id;

/**
 * Performs the basic initialization for the clients. Like short
 */
void init_client() {
	//populate the server addr structure
	//first wait till the process read the server port information
	// REUSED CODE :- http://www.tldp.org/LDP/lpg/node81.html, http://www.cs.cf.ac.uk/Dave/C/node27.html
	//--START
	client_key = ftok("./test", 'S');	//5678;
	/*
	 * Locate the segment.
	 */
	if ((client_shmid = shmget(client_key, SEGSIZE, 0666)) < 0) {
		perror("shmget");
		exit(1);
	}

	/*
	 * Now we attach the segment to our data space.
	 */
	if ((client_shm = shmat(client_shmid, NULL, 0)) == (char *) -1) {
		perror("shmat");
		exit(1);
	}

	// --END

	//setup the sockets for TCP and UDP communications
	//set up TCP connection
	memset(&client_tcp_server, 0, sizeof(client_tcp_server));
	tcp_client_sock_fd = create_tcp_socket();

	//set the udp connection
	memset(&udp_host_sock_addr, 0, sizeof(udp_host_sock_addr));
	udp_sock_fd = create_udp_socket();
	populate_sockaddr_in(&udp_host_sock_addr, "localhost", local_udp_port);
	if (bind_address(udp_sock_fd, udp_host_sock_addr) < 0) {
		perror("Error biding the address to socket. Exiting!!");
		exit(0);
	}
}

void destroy_client() {
	FD_ZERO(&readfds);
	close(udp_sock_fd);
	close(tcp_client_sock_fd);
}

int getMax(int a, int b) {
	int c = a - b;
	int k = (c >> 31) & 0x1;
	int max = a - k * c;
	return max;
}

/**
 * This function calculates the triad identity for the client
 */
void get_triad_id(int nonce, char* client_name) {
	int name_length = strlen(client_name);
	int buffer_length = sizeof(int) + name_length;
	unsigned char *buffer = malloc(buffer_length - 1);

	unsigned int n = htonl(nonce);
	buffer[3] = (n >> 24) & 0xFF;
	buffer[2] = (n >> 16) & 0xFF;
	buffer[1] = (n >> 8) & 0xFF;
	buffer[0] = n & 0xFF;

	int i = 4;
	for (i = 4; i < buffer_length; i++) {
		buffer[i] = (unsigned char) client_name[i - 4];
	}
	self_triad_id = projb_hash(buffer, buffer_length);
	free(buffer);

	printf("get_triad_id: the triad id for client: %s is= 0x%x\n", client_name,
			self_triad_id);
}

/**
 * This function handles the data received at the TCP socket
 */
void handle_tcp_receives() {
	int pid = getpid(); // to be sent to the manager.

	if (recv(tcp_client_sock_fd, buffer, MAXSIZE - 1, 0) < 0) {
		perror("Error in receiving data for server");
	}

	switch (atoi(buffer)) { // will covert the string till the new line character to integer
	case 1292:
		//tokenize the data received from the manager
		sscanf(buffer, "%d\n%d\n%ld\n%s\n%d\n%s", &status, &stage, &nonce,
				client_name, &client_1_port_no, second_name);
		printf("do_client: data received from server: \n%d\n%ld\n%s\n%d\n%s\n",
				stage, nonce, client_name, client_1_port_no, second_name);

		//prepare the client output file name
		sprintf(client_output_filename, "stage%d.%s.out", stage, client_name);
		printf("do_client: the client output fille name is=%s\n",
				client_output_filename);
		client_output_fd = open_file(client_output_filename, "w");

		//caculate the triad id of this client
		get_triad_id(nonce, client_name);

		//The first client is its predecessor and successor otherwise client
		//should send messages to messages in the triad ring to find its location
		if (client_1_port_no == 0) {
			predecessor_triad_id = self_triad_id;
			successor_triad_id = self_triad_id;
		} else {
			//TODO request client 1 for the successor and predecessor

		}

		//after joining the triad ring log to output file
		fprintf(client_output_fd, "client %s created with hash 0x%x", client_name,
				self_triad_id);
		fflush(client_output_fd);

		// fetch the local port number and send back to the manager
		socklen_t addrlen = sizeof(udp_host_sock_addr);
		int local_port = 0;
		if (getsockname(udp_sock_fd, (struct sockaddr *) &udp_host_sock_addr,
				&addrlen) == 0) {
			local_port = ntohs(udp_host_sock_addr.sin_port);
		}

		//calculate the new nonce
		nonce += pid;
		printf("do_client: computed nounce is: %ld\n", nonce);

		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "%ld %d\n%d\n", nonce, pid, local_port);
		printf("do_client: local udp port number = %d\n", local_port);
		if (send(tcp_client_sock_fd, buffer, MAXSIZE - 1, 0) < 0) {
			perror("Error sending data to server");
		}

		break;
	case 557:
		printf("handle_tcp_receives: store \n");
		break;
	case 630:
		printf("handle_tcp_receives: search \n");
		break;
	}

}

/**
 * This function handles the data received at the UDP socket
 */
void handle_udp_receives() {

}

/**
 * This function handles the I/O synchronously between multiple socket
 * file descriptors
 */
void handle_io_synchronously(int tcp_sock_fd, int udp_sock_fd) {
	int kill_io = 0, rv;
	//initialize the select
	FD_ZERO(&readfds);
	// add TCP and UDP descriptors to the set
	FD_SET(tcp_client_sock_fd, &readfds);
	FD_SET(udp_sock_fd, &readfds);

	//keep listening for incoming message on any socket file descriptors
	do {
		//wait only for reading purpose
		rv = select(getMax(udp_sock_fd, tcp_client_sock_fd) + 1, &readfds, NULL,
		NULL, NULL);
		if (rv == -1) {
			perror("select");
			break;
		} else {
			//if TCP socket receives data
			if (FD_ISSET(tcp_client_sock_fd, &readfds)) {
				handle_tcp_receives();
			}
			//if UDP socket receives data
			if (FD_ISSET(udp_sock_fd, &readfds)) {
				handle_udp_receives();
			}
		}

	} while (kill_io == 0);
}

/**
 * REad from the shared memroy
 */
int readshm(char *segptr, char *s) {
	memset(s, 0, SEGSIZE);
	strcpy(s, segptr);
	return segptr == NULL;
}

/**
 * This function is the starting point of the client.
 */
void do_client() {

	int server_port = 0;

	memset(buffer, 0, sizeof(buffer));

	// initialize the client application
	init_client();

	//connect to the manager
	do {
		readshm(client_shm, s);
		printf("do_client: server port number read from shared memory is: %s\n",
				s);
		printf("do_client: trying to connect the server\n");
		server_port = atoi(s);
		populate_sockaddr_in(&client_tcp_server, "localhost", server_port);
	} while (connect(tcp_client_sock_fd, (struct sockaddr *) &client_tcp_server,
			sizeof(client_tcp_server)) < 0);

	//ideal spot to put select() for synchronously hanlding the I/O
	handle_io_synchronously(tcp_client_sock_fd, udp_sock_fd);

}

