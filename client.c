/*
 * client.c
 *
 *  Created on: Oct 24, 2013
 *      Author: csci551
 */

#include "client.h"
#include "sha1.h"

char client_output_filename[100], client_name[80], port_no[6], second_name[80];
int stage;
long nonce;
unsigned int triad_id;

int readshm(char *segptr, char *s) {
	memset(s, 0, SEGSIZE);
	strcpy(s, segptr);
	return segptr == NULL;
}

/**
 * This function calculates the triad identity for the client
 */
void get_triad_id(int nonce, char* client_name) {
	int name_length = strlen(client_name);
	int buffer_length = sizeof(int) + name_length;
	char buffer[buffer_length];
	memset(buffer, 0, sizeof(buffer));
	unsigned int n = htonl(nonce);
	sprintf(buffer, "%u%s", n, client_name);

	/*unsigned char final_buff[strlen(buffer)];
	memcpy(final_buff, buffer, strlen(buffer));*/
	triad_id = projb_hash((unsigned char*)buffer, buffer_length);
	printf("get_triad_id: the triad id for client: %s is= %x\n", client_name,
			triad_id);
}

/**
 * This function is the starting point of the client.
 */
void do_client() {

	//printf("I am child process!!\n");
	int shmid, server_port = 0;
	key_t key;
	char *shm, s[SEGSIZE], buffer[MAXSIZE];
	memset(buffer, 0, sizeof(buffer));

	int pid = getpid(); // to be sent to the server.

	//set up TCP connection
	struct sockaddr_in tcp_server;
	memset(&tcp_server, 0, sizeof(tcp_server));
	int tcp_client_sock_fd = create_tcp_socket();

	//populate the server addr structure
	//first wait till the process read the server port information
	// REUSED CODE :- http://www.tldp.org/LDP/lpg/node81.html, http://www.cs.cf.ac.uk/Dave/C/node27.html
	//--START
	key = ftok("./test", 'S');	//5678;
	/*
	 * Locate the segment.
	 */
	if ((shmid = shmget(key, SEGSIZE, 0666)) < 0) {
		perror("shmget");
		exit(1);
	}

	/*
	 * Now we attach the segment to our data space.
	 */
	if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
		perror("shmat");
		exit(1);
	}

	// --END

	//connect to the server
	do {
		readshm(shm, s);
		printf("do_client: server port number read from shared memory is: %s\n",
				s);
		printf("do_client: trying to connect the server\n");
		server_port = atoi(s);
		populate_sockaddr_in(&tcp_server, "localhost", server_port);
	} while (connect(tcp_client_sock_fd, (struct sockaddr *) &tcp_server,
			sizeof(tcp_server)) < 0);

	if (recv(tcp_client_sock_fd, buffer, MAXSIZE - 1, 0) < 0) {
		perror("Error in receiving data for server");
	}

	//tokenize the data received from the manager
	sscanf(buffer, "%d\n%ld\n%s\n%s\n%s", &stage, &nonce, client_name, port_no,
			second_name);
	printf("do_client: data received from server: \n%d\n%ld\n%s\n%s\n%s\n",
			stage, nonce, client_name, port_no, second_name);

	//caculate the triad id of this client
//	get_triad_id(nonce, client_name);
	get_triad_id(1234, "foo");

	nonce += pid;
	printf("do_client: computed nounce is: %ld\n", nonce);

	//send this information to the server
	memset(buffer, 0, sizeof(buffer));
	sprintf(buffer, "%ld %d\n", nonce, pid);
	printf("do_client: data to send to manager: %s\n", buffer);
	if (send(tcp_client_sock_fd, buffer, MAXSIZE - 1, 0) < 0) {
		perror("Error sending data to server");
	}

	//perror("ERROR connecting");

	close(tcp_client_sock_fd);

}

