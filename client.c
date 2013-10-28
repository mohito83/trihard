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
int stage, status, client_shmid, tcp_client_sock_fd, udp_sock_fd;
struct sockaddr_in client_tcp_server, udp_host_sock_addr;
key_t client_key;
long nonce, new_nonce = 0;
FILE *client_output_fd;
//triad ids
unsigned int self_triad_id, predecessor_triad_id = -1, successor_triad_id = -1;
//port numbers
int successor_udp_port, predecessor_udp_port, local_udp_port = 0,
		client_1_udp_port_no;

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

void destroy_client(fd_set readfds) {
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
unsigned int get_triad_id(int nonce, char* client_name) {
	int triad_id = -1;
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
	triad_id = projb_hash(buffer, buffer_length);
	free(buffer);

	return triad_id;
}

/**
 * This function makes the decision regarding the next step for a node to take
 * while booting the triad ring
 *
 * 100 => position found update the predecessor and successor in the ring.
 * 101 => self_id is to be added at the beginning
 * 102 => self_id is to be added at the end
 * 103 => only one element in the ring
 *
 * @return decision
 */
int decision_matrix(int self_id, int client1_id, int successor_id) {
	int decision = -1;
	if (client1_id == successor_id) {
		decision = 103;
		return decision;
	}
	if (client1_id < self_id && self_id < successor_id) {
		decision = 100;
		return decision;
	}
	if (client1_id > successor_id && self_id < successor_id) {
		decision = 101;
		return decision;
	}
	if (client1_id > successor_id && self_id > successor_id
			&& client1_id < self_id) {
		decision = 102;
		return decision;
	}
	printf("decision_matrix: the decision is= %d\n", decision);

	return decision;
}

/**
 * This function the returns the triad if of the successor and logs the
 * transaction in the log file.
 * @triad_id: triad id of the target node
 * @port_no : UDP port number of the target node
 */
void successor_q(int triad_id, int dest_port_no) {
	//log the event in the log file
	//successor-q sent (0xa9367d92)
	fprintf(client_output_fd, "successor-q sent (0x%x)\n", triad_id);
	fflush(client_output_fd);
	printf("successor_q:: successor-q sent (0x%x)\n", triad_id);

	int status = 1;
	int size = sizeof(status) + sizeof(triad_id);
	unsigned char mssg[size];
	struct sockaddr_in dest_addrin;

	status = htonl(status);
	triad_id = htonl(triad_id);

	memset(mssg, 0, size);
	memcpy(mssg, &status, sizeof(status));
	memcpy(mssg + sizeof(status), &triad_id, sizeof(triad_id));

	populate_sockaddr_in(&dest_addrin, "localhost", dest_port_no);

	sendto(udp_sock_fd, mssg, size, 0, (struct sockaddr*) &dest_addrin,
			sizeof(dest_addrin));

}

/**
 * This function replies back to the successor request
 * @dest_addr: socket address information for the destination
 * @self_triad_id: own triad id
 * @successor_triad_id:successor's triad id
 * @successor_udp_port: successor's udp port
 * @return successor's triad id
 */
void successor_r(struct sockaddr dest_addr, int self_triad_id,
		int successor_triad_id, int successor_udp_port) {
	//log the event in the log file
	fprintf(client_output_fd, "successor-r sent (0x%x 0x%x %d)\n",
			self_triad_id, successor_triad_id, successor_udp_port);
	fflush(client_output_fd);
	printf("successor_r:: successor-r sent (0x%x 0x%x %d)\n", self_triad_id,
			successor_triad_id, successor_udp_port);

	int status = 2, pointer = 0;
	unsigned char buff[MAXSIZE];
	memset(buff, 0, MAXSIZE);

	status = htonl(status);
	self_triad_id = htonl(self_triad_id);
	successor_triad_id = htonl(successor_triad_id);
	successor_udp_port = htonl(successor_udp_port);

	memcpy(buff + pointer, &status, sizeof(status));
	pointer += sizeof(status);
	memcpy(buff + pointer, &self_triad_id, sizeof(self_triad_id));
	pointer += sizeof(self_triad_id);
	memcpy(buff + pointer, &successor_triad_id, sizeof(successor_triad_id));
	pointer += sizeof(successor_triad_id);
	memcpy(buff + pointer, &successor_udp_port, sizeof(successor_udp_port));
	pointer += sizeof(successor_udp_port);

	sendto(udp_sock_fd, buff, pointer, 0, (struct sockaddr*) &dest_addr,
			sizeof(dest_addr));

}

/**
 * This function the returns the triad if of the predecessor and logs the
 * transaction in the log file.
 * @triad_id: triad id of the target node
 * @port_no : UDP port number of the target node
 */
void predecessor_q(int triad_id, int dest_port_no) {
	//log the event in the log file
	//successor-q sent (0xa9367d92)
	fprintf(client_output_fd, "predecessor-q sent (0x%x)\n", triad_id);
	fflush(client_output_fd);
	printf("predecessor_q:: predecessor-q sent (0x%x)\n", triad_id);

	int status = 3;
	int size = sizeof(status) + sizeof(triad_id);
	unsigned char mssg[size];
	struct sockaddr_in dest_addrin;

	status = htonl(status);
	triad_id = htonl(triad_id);

	memset(mssg, 0, size);
	memcpy(mssg, &status, sizeof(status));
	memcpy(mssg + sizeof(status), &triad_id, sizeof(triad_id));

	populate_sockaddr_in(&dest_addrin, "localhost", dest_port_no);

	sendto(udp_sock_fd, mssg, size, 0, (struct sockaddr*) &dest_addrin,
			sizeof(dest_addrin));

}

/**
 * This function replies back to the successor request
 * @dest_addr: socket address information for the destination
 * @self_triad_id: own triad id
 * @predecessor_triad_id:successor's triad id
 * @predecessor_udp_port: successor's udp port
 * @return successor's triad id
 */
void predecessor_r(struct sockaddr dest_addr, int self_triad_id,
		int predecessor_triad_id, int predecessor_udp_port) {
	//log the event in the log file
	fprintf(client_output_fd, "predecessor-r sent (0x%x 0x%x %d)\n",
			self_triad_id, predecessor_triad_id, predecessor_udp_port);
	fflush(client_output_fd);
	printf("predecessor_r:: predecessor-r sent (0x%x 0x%x %d)\n", self_triad_id,
			predecessor_triad_id, predecessor_udp_port);

	int status = 4, pointer = 0;
	unsigned char buff[MAXSIZE];
	memset(buff, 0, MAXSIZE);

	status = htonl(status);
	self_triad_id = htonl(self_triad_id);
	predecessor_triad_id = htonl(predecessor_triad_id);
	predecessor_udp_port = htonl(predecessor_udp_port);

	memcpy(buff + pointer, &status, sizeof(status));
	pointer += sizeof(status);
	memcpy(buff + pointer, &self_triad_id, sizeof(self_triad_id));
	pointer += sizeof(self_triad_id);
	memcpy(buff + pointer, &predecessor_triad_id, sizeof(predecessor_triad_id));
	pointer += sizeof(predecessor_triad_id);
	memcpy(buff + pointer, &predecessor_udp_port, sizeof(predecessor_udp_port));
	pointer += sizeof(predecessor_udp_port);

	sendto(udp_sock_fd, buff, pointer, 0, (struct sockaddr*) &dest_addr,
			sizeof(dest_addr));

}

/**
 * This function updates the finger table or the predecessor/successor entries
 * of the node identified by the dest_port_no
 * @dest_port_no
 * @triad_id of the target node
 * @new_triad_id new successor or predecessor triad id
 * @new_port new successor or predecessor port number
 * @flag to decide to affect successor or predecessor fields of the target nodes.
 * 		'0' for predecessor & '1' for successor
 */
void update_q(int dest_port_no, int triad_id, int new_triad_id, int new_port,
		int flag) {
	//log the event in the log file
	fprintf(client_output_fd, "update-q sent (0x%x 0x%x %d %d)\n", triad_id,
			new_triad_id, new_port, flag);
	fflush(client_output_fd);
	printf("update-q:: update-q sent (0x%x 0x%x %d %d)\n", triad_id,
			new_triad_id, new_port, flag);

	int status = 7, pointer = 0;
	unsigned char buff[MAXSIZE];
	memset(buff, 0, MAXSIZE);

	status = htonl(status);
	triad_id = htonl(triad_id);
	new_triad_id = htonl(new_triad_id);
	new_port = htonl(new_port);
	flag = htonl(flag);

	memcpy(buff + pointer, &status, sizeof(status));
	pointer += sizeof(status);
	memcpy(buff + pointer, &triad_id, sizeof(triad_id));
	pointer += sizeof(triad_id);
	memcpy(buff + pointer, &new_triad_id, sizeof(new_triad_id));
	pointer += sizeof(new_triad_id);
	memcpy(buff + pointer, &new_port, sizeof(new_port));
	pointer += sizeof(new_port);
	memcpy(buff + pointer, &flag, sizeof(flag));
	pointer += sizeof(flag);

	struct sockaddr_in dest_addrin;

	populate_sockaddr_in(&dest_addrin, "localhost", dest_port_no);

	sendto(udp_sock_fd, buff, pointer, 0, (struct sockaddr*) &dest_addrin,
			sizeof(dest_addrin));
}

/**
 * Sends back the update response
 */
void update_r(struct sockaddr dest_addr, int result, int self_triad_id,
		int successor_triad_id, int successor_udp_port, int flag) {
	//log the event in the log file
	fprintf(client_output_fd, "update-r sent (0x%x %d 0x%x %d %d)\n",
			self_triad_id, result, successor_triad_id, successor_udp_port,
			flag);
	fflush(client_output_fd);
	printf("update-r:: update-r sent (0x%x %d 0x%x %d %d)\n", self_triad_id,
			result, successor_triad_id, successor_udp_port, flag);

	int status = 8, pointer = 0;
	unsigned char buff[MAXSIZE];
	memset(buff, 0, MAXSIZE);

	status = htonl(status);
	self_triad_id = htonl(self_triad_id);
	result = htonl(result);
	successor_triad_id = htonl(successor_triad_id);
	successor_udp_port = htonl(successor_udp_port);
	flag = htonl(flag);

	memcpy(buff + pointer, &status, sizeof(status));
	pointer += sizeof(status);
	memcpy(buff + pointer, &self_triad_id, sizeof(self_triad_id));
	pointer += sizeof(self_triad_id);
	memcpy(buff + pointer, &result, sizeof(result));
	pointer += sizeof(result);
	memcpy(buff + pointer, &successor_triad_id, sizeof(successor_triad_id));
	pointer += sizeof(successor_triad_id);
	memcpy(buff + pointer, &successor_udp_port, sizeof(successor_udp_port));
	pointer += sizeof(successor_udp_port);
	memcpy(buff + pointer, &flag, sizeof(flag));
	pointer += sizeof(flag);

	sendto(udp_sock_fd, buff, pointer, 0, (struct sockaddr*) &dest_addr,
			sizeof(dest_addr));
}

/**
 * This function adds the node to the triad ring
 */
void add_to_ring(char * client1_name, int client_1_udp_port_no) {
	printf("add_to_ring: adding client %s to the triad ring\n", client1_name);
	int client1_triad_id = get_triad_id(nonce, client1_name);

	//decision matrix
	successor_q(client1_triad_id, client_1_udp_port_no);
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
				client_name, &client_1_udp_port_no, second_name);
		printf("do_client: data received from server: \n%d\n%ld\n%s\n%d\n%s\n",
				stage, nonce, client_name, client_1_udp_port_no, second_name);

		//prepare the client output file name
		sprintf(client_output_filename, "stage%d.%s.out", stage, client_name);
		printf("do_client: the client output fille name is=%s\n",
				client_output_filename);
		client_output_fd = open_file(client_output_filename, "w");

		//caculate the triad id of this client
		self_triad_id = get_triad_id(nonce, client_name);

		//The first client is its predecessor and successor otherwise client
		//should send messages to messages in the triad ring to find its location
		if (client_1_udp_port_no == 0) {
			predecessor_triad_id = self_triad_id;
			successor_triad_id = self_triad_id;
			//after joining the triad ring log to output file
			fprintf(client_output_fd, "client %s created with hash 0x%x\n",
					client_name, self_triad_id);
			fflush(client_output_fd);
		} else {
			add_to_ring(second_name, client_1_udp_port_no);
		}
		printf(
				"do_client: predecessor_triad_id=0x%x\tsuccessor_triad_id0x=%x\n",
				predecessor_triad_id, successor_triad_id);

		// fetch the local port number and send back to the manager
		socklen_t addrlen = sizeof(udp_host_sock_addr);
		if (getsockname(udp_sock_fd, (struct sockaddr *) &udp_host_sock_addr,
				&addrlen) == 0) {
			local_udp_port = ntohs(udp_host_sock_addr.sin_port);
		}

		if (client_1_udp_port_no == 0) { // only in the case of first client
			predecessor_udp_port = local_udp_port;
			successor_udp_port = local_udp_port;
		}

		//calculate the new nonce
		new_nonce = nonce + pid;
		printf("do_client: computed nounce is: %ld\n", new_nonce);

		//TODO to be sent when the client is completely setup.. for the time
		//being let the code be there
		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "%ld %d\n%d\n", new_nonce, pid, local_udp_port);
		printf("do_client: local udp port number = %d\n", local_udp_port);
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
	int status = 0, data, pointer = 0, self_id, successor_id, successor_port,
			flag, result;
	unsigned char buff[MAXSIZE];
	struct sockaddr dest_addr;
	socklen_t dest_addr_len = sizeof(struct sockaddr);
	if (recvfrom(udp_sock_fd, buff, sizeof(buff), 0,
			(struct sockaddr*) &dest_addr, &dest_addr_len) < 0) {
		perror("Error receiving Triad messages\n");
	}

	memcpy(&status, buff, sizeof(status));
	status = ntohl(status);
	pointer += sizeof(status);

	switch (status) {
	case 1:
		data = 0;
		memcpy(&data, buff + pointer, sizeof(data));
		data = ntohl(data);
		//log the transaction in the log file
		fprintf(client_output_fd, "successor-q received (0x%x)\n", data);
		fflush(client_output_fd);

		//reply with successor-r message
		successor_r(dest_addr, self_triad_id, successor_triad_id,
				successor_udp_port);

		printf("handle_udp_receives: successor-q received (0x%x)\n", data);
		break;

	case 2:
		memcpy(&self_id, buff + pointer, sizeof(self_id));
		pointer += sizeof(self_id);
		memcpy(&successor_id, buff + pointer, sizeof(successor_id));
		pointer += sizeof(successor_id);
		memcpy(&successor_port, buff + pointer, sizeof(successor_port));

		self_id = ntohl(self_id);
		successor_id = ntohl(successor_id);
		successor_port = ntohl(successor_port);

		//log the transaction in the log file
		fprintf(client_output_fd, "successor-r received (0x%x 0x%x %d)\n",
				self_id, successor_id, successor_port);
		fflush(client_output_fd);
		printf("handle_udp_receives: successor-r received (0x%x 0x%x %d)\n",
				self_id, successor_id, successor_port);

		//make a decision about location of this node in the triad ring
		result = decision_matrix(self_triad_id,
				get_triad_id(nonce, second_name), successor_id);
		//result =103;

		/* 100 => position found update the predecessor and successor in the ring.
		 * 101 => self_id is to be added at the beginning
		 * 102 => self_id is to be added at the end
		 * 103 => only one element in the ring
		 */

		switch (result) {
		case 100:

			break;
		case 101:

			break;
		case 102:

			break;
		case 103:
			//send to update messages to the first node. First message to update
			//its predecessor entries and second message for its successor entries
			update_q(client_1_udp_port_no, get_triad_id(nonce, second_name),
					self_triad_id, local_udp_port, 0);
			update_q(client_1_udp_port_no, get_triad_id(nonce, second_name),
					self_triad_id, local_udp_port, 1);
			break;
		}

		break;

	case 7:
		memcpy(&self_id, buff + pointer, sizeof(self_id));
		pointer += sizeof(self_id);
		memcpy(&successor_id, buff + pointer, sizeof(successor_id));
		pointer += sizeof(successor_id);
		memcpy(&successor_port, buff + pointer, sizeof(successor_port));
		pointer += sizeof(successor_port);
		memcpy(&flag, buff + pointer, sizeof(flag));

		self_id = ntohl(self_id);
		successor_id = ntohl(successor_id);
		successor_port = ntohl(successor_port);
		flag = ntohl(flag);

		//log the transaction in the log file
		fprintf(client_output_fd, "update-q received (0x%x 0x%x %d %d)\n",
				self_id, successor_id, successor_port, flag);
		fflush(client_output_fd);
		printf("handle_udp_receives: update-q received (0x%x 0x%x %d %d)\n",
				self_id, successor_id, successor_port, flag);

		//reply back to the sender
		result = 1;
		update_r(dest_addr, result, self_triad_id, successor_triad_id,
				successor_port, flag);

		//update the node properties
		if (flag) {
			successor_udp_port = successor_port;
			successor_triad_id = successor_id;
		} else {
			predecessor_udp_port = successor_port;
			predecessor_triad_id = successor_id;
		}

		break;

	case 8:
		memcpy(&self_id, buff + pointer, sizeof(self_id));
		pointer += sizeof(self_id);
		memcpy(&result, buff + pointer, sizeof(result));
		pointer += sizeof(result);
		memcpy(&successor_id, buff + pointer, sizeof(successor_id));
		pointer += sizeof(successor_id);
		memcpy(&successor_port, buff + pointer, sizeof(successor_port));
		pointer += sizeof(successor_id);
		memcpy(&flag, buff + pointer, sizeof(flag));

		self_id = ntohl(self_id);
		result = ntohl(result);
		successor_id = ntohl(successor_id);
		successor_port = ntohl(successor_port);
		flag = ntohl(flag);

		//log the transaction in the log file
		fprintf(client_output_fd, "update-r received (0x%x %d 0x%x %d %d)\n",
				self_id, result, successor_id, successor_port, flag);
		fflush(client_output_fd);
		printf("handle_udp_receives: update-r received (0x%x %d 0x%x %d %d)\n",
				self_id, result, successor_id, successor_port, flag);
		if (result) {
			if (flag) {
				successor_udp_port = successor_port;
				successor_triad_id = successor_id;
			} else {
				predecessor_udp_port = successor_port;
				predecessor_triad_id = successor_id;
			}
		}
		break;
	}
}

/**
 * This function handles the I/O synchronously between multiple socket
 * file descriptors
 */
fd_set handle_io_synchronously(int tcp_sock_fd, int udp_sock_fd) {
	fd_set readfds;
	int kill_io = 0, rv;

	//keep listening for incoming message on any socket file descriptors
	do {
		//initialize the select
		FD_ZERO(&readfds);
		// add TCP and UDP descriptors to the set
		FD_SET(tcp_client_sock_fd, &readfds);
		FD_SET(udp_sock_fd, &readfds);

		//wait only for reading purpose
		rv = select(getMax(udp_sock_fd, tcp_client_sock_fd) + 1, &readfds, NULL,
		NULL, NULL);
		if (rv == -1) {
			perror("select");
			break;
		} else {
			//if TCP socket receives data
			if (FD_ISSET(tcp_client_sock_fd, &readfds)) {
				//printf("handle_io_synchronously: Received TCP message\n");
				handle_tcp_receives();
			}
			//if UDP socket receives data
			if (FD_ISSET(udp_sock_fd, &readfds)) {
				printf("handle_io_synchronously: Received UDP message\n");
				handle_udp_receives();
			}
		}

	} while (kill_io == 0);

	return readfds;
}

/**
 * Read from the shared memroy
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
	fd_set readfds = handle_io_synchronously(tcp_client_sock_fd, udp_sock_fd);
	destroy_client(readfds);

}
