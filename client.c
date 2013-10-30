/*
 * client.c
 *
 *  Created on: Oct 24, 2013
 *      Author: csci551
 */

#include "client.h"
#include "sha1.h"
#include "file_io_op.h"

char client_output_filename[100], str[80];
char *client_shm;
int status, client_shmid, tcp_client_sock_fd, udp_sock_fd;
struct sockaddr_in client_tcp_server, udp_host_sock_addr;
key_t client_key;
FILE *client_output_fd;

//TODO : handle the out of memory error. At OM error the manager as well as all clients should cease to exist

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
 * Prints the status of the client
 */
void print_client_status(triad_client *client) {
	printf(
			"CLIENT_STATUS::\tnode_name=%s\n\tself_triad_id=0x%x\tself_udp_port=%d\n\tpredecessor_triad_id=0x%x\tpredecessor_udp_port=%d\n\tsuccessor_triad_id=0x%x\tsuccessor_udp_port=%d\n",
			client->name, client->self_triad_id, client->local_udp_port,
			client->predecessor_triad_id, client->predecessor_udp_port,
			client->successor_triad_id, client->successor_udp_port);
}

/**
 * This function checks if the given client is fully added to the triad ring.
 */
int is_node_added_to_ring(triad_client *client) {
	return client->local_udp_port && client->successor_udp_port
			&& client->predecessor_udp_port;
}

/**
 * This function adds data to given client node.
 * @client : triad client node
 * @data : data to be stored
 * @len : length of the data
 * @return 2,if data alread present. 1, if data is successfully added.
 * 			0, if data addition fails
 */
int add_data_to_client(triad_client *client, char *data, int len) {
	int result = 0;
	if (is_data_hash_present(client, get_triad_id(client->nonce, data))) {
		result = 2;
		return result;
	}
	//TODO check if we can still get correct output without using the len argument
	result = My402ListAppend(&(client->data_list), data);
	return result;
}

/**
 * This function will find if the data hash is present with the client or not
 * @hash_to_compare
 * @return 1 for success 0 for failure
 */
int is_data_hash_present(triad_client *client, unsigned int hash_to_comapre) {
	int success = 0;
	if (My402ListEmpty(&(client->data_list))) {
		return success;
	}
	My402ListElem* p = (client->data_list.anchor.next);
	while (p != NULL && p != &(client->data_list.anchor)) {
		if (get_triad_id(client->nonce, (char*) p->obj) == hash_to_comapre) {
			success = 1;
			break;
		}
		p = p->next;
	}
	return success;
}

/**
 * Performs the basic initialization for the clients. Like short
 */
void init_client(triad_client *client) {
	My402ListInit(&client->data_list);
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
	populate_sockaddr_in(&udp_host_sock_addr, "localhost",
			client->local_udp_port);
	if (bind_address(udp_sock_fd, udp_host_sock_addr) < 0) {
		perror("Error biding the address to socket. Exiting!!");
		exit(0);
	}
}

void destroy_client(fd_set readfds) {
	FD_ZERO(&readfds);
	close(udp_sock_fd);
	close(tcp_client_sock_fd);
	fclose(client_output_fd);
}

int getMax(int a, int b) {
	int c = a - b;
	int k = (c >> 31) & 0x1;
	int max = a - k * c;
	return max;
}

/**
 * This function makes the decision regarding the next step for a node to take
 * while booting the triad ring
 *
 * 100 => position found update the predecessor and successor in the ring.
 * 101 => self_id is to be added at the beginning
 * 102 => self_id is to be added at the end
 * 103 => only one element in the ring
 * 104 => keep searching for right neighborhood to fit in
 *
 * @return decision
 */
int decision_matrix(unsigned int self_id, unsigned int client1_id,
		unsigned int successor_id, unsigned int predecessor_id) {
	int decision = -1;
	if (predecessor_id == successor_id) {
		decision = 103;
	} else if (predecessor_id < self_id && self_id < successor_id) {
		decision = 100;
		/*} else if (client1_id > successor_id && self_id < successor_id) {
		 decision = 101;
		 } else if (client1_id > successor_id && self_id > successor_id
		 && client1_id < self_id) {
		 decision = 102;*/

		//this is the boundary condition and the list rollovers
	} else if (predecessor_id > successor_id
			&& (successor_id > self_id || predecessor_id < self_id)) {
		decision = 102;
	} else {
		decision = 104;
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
void successor_q(triad_client *client, unsigned int triad_id,
		unsigned int dest_port_no) {
	//log the event in the log file
	//successor-q sent (0xa9367d92)
	fprintf(client_output_fd, "successor-q sent (0x%x)\n", triad_id);
	fflush(client_output_fd);
	printf("successor_q:: client=%s\tsuccessor-q sent (0x%x)\n", client->name,
			triad_id);

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

	print_client_status(client);
}

/**
 * This function replies back to the successor request
 * @dest_addr: socket address information for the destination
 * @self_triad_id: own triad id
 * @successor_triad_id:successor's triad id
 * @successor_udp_port: successor's udp port
 * @return successor's triad id
 */
void successor_r(struct sockaddr_in dest_addr, triad_client *client,
		unsigned int self_triad_id, unsigned int successor_triad_id,
		unsigned int successor_udp_port) {
	//log the event in the log file
	fprintf(client_output_fd, "successor-r sent (0x%x 0x%x %d)\n",
			self_triad_id, successor_triad_id, successor_udp_port);
	fflush(client_output_fd);
	printf("successor_r:: client=%s\tsuccessor-r sent (0x%x 0x%x %d)\n",
			client->name, self_triad_id, successor_triad_id,
			successor_udp_port);

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

	print_client_status(client);
}

/**
 * This function the returns the triad if of the predecessor and logs the
 * transaction in the log file.
 * @triad_id: triad id of the target node
 * @port_no : UDP port number of the target node
 */
void predecessor_q(triad_client *client, int triad_id, int dest_port_no) {
	//log the event in the log file
	//successor-q sent (0xa9367d92)
	fprintf(client_output_fd, "predecessor-q sent (0x%x)\n", triad_id);
	fflush(client_output_fd);
	printf("predecessor_q:: client=%s\tpredecessor-q sent (0x%x)\n",
			client->name, triad_id);

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

	print_client_status(client);
}

/**
 * This function replies back to the successor request
 * @dest_addr: socket address information for the destination
 * @self_triad_id: own triad id
 * @predecessor_triad_id:successor's triad id
 * @predecessor_udp_port: successor's udp port
 * @return successor's triad id
 */
void predecessor_r(struct sockaddr dest_addr, triad_client *client,
		int self_triad_id, int predecessor_triad_id, int predecessor_udp_port) {
	//log the event in the log file
	fprintf(client_output_fd, "predecessor-r sent (0x%x 0x%x %d)\n",
			self_triad_id, predecessor_triad_id, predecessor_udp_port);
	fflush(client_output_fd);
	printf("predecessor_r:: client=%s\tpredecessor-r sent (0x%x 0x%x %d)\n",
			client->name, self_triad_id, predecessor_triad_id,
			predecessor_udp_port);

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

	print_client_status(client);
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
void update_q(triad_client *client, int dest_port_no, int triad_id,
		int new_triad_id, int new_port, int flag) {
	//log the event in the log file
	fprintf(client_output_fd, "update-q sent (0x%x 0x%x %d %d)\n", triad_id,
			new_triad_id, new_port, flag);
	fflush(client_output_fd);
	printf("update-q:: client=%s\tupdate-q sent (0x%x 0x%x %d %d)\n",
			client->name, triad_id, new_triad_id, new_port, flag);

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

	print_client_status(client);
}

/**
 * Sends back the update response
 */
void update_r(struct sockaddr_in dest_addr, triad_client *client, int result,
		int self_triad_id, int successor_triad_id, int successor_udp_port,
		int flag) {
	//log the event in the log file
	fprintf(client_output_fd, "update-r sent (0x%x %d 0x%x %d %d)\n",
			self_triad_id, result, successor_triad_id, successor_udp_port,
			flag);
	fflush(client_output_fd);
	printf("update-r:: client=%s\tupdate-r sent (0x%x %d 0x%x %d %d)\n",
			client->name, self_triad_id, result, successor_triad_id,
			successor_udp_port, flag);

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

	print_client_status(client);
}

/**
 * This function sends stores-q request messages to other nodes to find the
 * closest possible match, to store the data.
 */
void stores_q(triad_client *client, unsigned int sucessor_id,
		unsigned int dest_port_no, char *str) {
	int status = 5, pointer = 0;
	unsigned int data_hash = get_triad_id(client->nonce, str);

	//log the event
	fprintf(client_output_fd, "stores-q sent (0x%x 0x%x)\n", sucessor_id,
			data_hash);
	fflush(client_output_fd);
	printf("stores-q:: client=%s\tstores-q sent (0x%x 0x%x) at port:%d\n",
			client->name, sucessor_id, data_hash, dest_port_no);

	status = htonl(status);
	sucessor_id = htonl(sucessor_id);
	data_hash = htonl(data_hash);

	char buff[MAXSIZE];
	memset(buff, 0, sizeof(buff));
	memcpy(buff + pointer, &status, sizeof(status));
	pointer += sizeof(status);
	memcpy(buff + pointer, &sucessor_id, sizeof(sucessor_id));
	pointer += sizeof(sucessor_id);
	memcpy(buff + pointer, &data_hash, sizeof(data_hash));
	pointer += sizeof(data_hash);

	struct sockaddr_in dest_addrin;

	populate_sockaddr_in(&dest_addrin, "localhost", dest_port_no);

	sendto(udp_sock_fd, buff, pointer, 0, (struct sockaddr*) &dest_addrin,
			sizeof(dest_addrin));
}

/**
 * Tjhis function replies back the stores-r message
 */
void stores_r(struct sockaddr_in dest_addr, triad_client *client,
		unsigned int self_id, unsigned int data_hash, unsigned int successor_id,
		unsigned int successor_port, int flag) {
	int status = 6, pointer = 0;
	char buff[MAXSIZE];
	memset(buff, 0, sizeof(buff));

	//log the event in the log file
	fprintf(client_output_fd, "stores-r sent (0x%x 0x%x 0x%x %d %d)\n", self_id,
			data_hash, successor_id, successor_port, flag);
	fflush(client_output_fd);
	printf("stores-r:: client=%s\tstores-r sent (0x%x 0x%x 0x%x %d %d)\n",
			client->name, self_id, data_hash, successor_id, successor_port,
			flag);

	status = htonl(status);
	self_id = htonl(self_id);
	data_hash = htonl(data_hash);
	successor_id = htonl(successor_id);
	successor_port = htonl(successor_port);
	flag = htonl(flag);

	memset(buff, 0, sizeof(buff));
	memcpy(buff + pointer, &status, sizeof(status));
	pointer += sizeof(status);
	memcpy(buff + pointer, &self_id, sizeof(self_id));
	pointer += sizeof(self_id);
	memcpy(buff + pointer, &data_hash, sizeof(data_hash));
	pointer += sizeof(data_hash);
	memcpy(buff + pointer, &successor_id, sizeof(successor_id));
	pointer += sizeof(successor_id);
	memcpy(buff + pointer, &successor_port, sizeof(successor_port));
	pointer += sizeof(successor_port);
	memcpy(buff + pointer, &flag, sizeof(flag));
	pointer += sizeof(flag);

	sendto(udp_sock_fd, buff, pointer, 0, (struct sockaddr*) &dest_addr,
			sizeof(dest_addr));
}

/**
 * This function sends the store-q message to the successor node.
 */
void store_q(triad_client *client, unsigned int successor_id,
		unsigned int successor_port, int len, char *str) {
	int status = 9, pointer = 0, tmp = len;
	char buff[MAXSIZE];
	memset(buff, 0, sizeof(buff));

	//log the event in the log file
	fprintf(client_output_fd, "store-q sent (0x%x %d %s)\n", successor_id, len,
			str);
	fflush(client_output_fd);
	printf("store-q:: client=%s\tstore-q sent (0x%x %d %s)\n", client->name,
			successor_id, len, str);

	status = htonl(status);
	successor_id = htonl(successor_id);
	len = htonl(len);

	memset(buff, 0, sizeof(buff));
	memcpy(buff + pointer, &status, sizeof(status));
	pointer += sizeof(status);
	memcpy(buff + pointer, &successor_id, sizeof(successor_id));
	pointer += sizeof(successor_id);
	memcpy(buff + pointer, &len, sizeof(len));
	pointer += sizeof(len);
	memcpy(buff + pointer, str, tmp);
	pointer += tmp;

	struct sockaddr_in dest_addrin;
	populate_sockaddr_in(&dest_addrin, "localhost", successor_port);
	sendto(udp_sock_fd, buff, pointer, 0, (struct sockaddr*) &dest_addrin,
			sizeof(dest_addrin));
}

/**
 * This function will send the store -r message
 */
void store_r(struct sockaddr_in dest_addr, triad_client *client,
		unsigned int self_id, unsigned int flag, unsigned int len, char *data) {
	int status = 10, pointer = 0, dlen = 0;
	char buff[MAXSIZE];
	memset(buff, 0, sizeof(buff));

	//log the event in the log file
	fprintf(client_output_fd, "store-r sent (0x%x %d %d %s)\n", self_id, flag,
			len, data);
	fflush(client_output_fd);
	printf("store-r:: client=%s\tstore-r sent (0x%x %d %d %s)\n", client->name,
			self_id, flag, len, data);

	status = htonl(status);
	self_id = htonl(self_id);
	//data = htonl(data);
	flag = htonl(flag);
	dlen = len;
	len = htonl(len);

	memset(buff, 0, sizeof(buff));
	memcpy(buff + pointer, &status, sizeof(status));
	pointer += sizeof(status);
	memcpy(buff + pointer, &self_id, sizeof(self_id));
	pointer += sizeof(self_id);
	memcpy(buff + pointer, &flag, sizeof(flag));
	pointer += sizeof(flag);
	memcpy(buff + pointer, &len, sizeof(len));
	pointer += sizeof(len);
	memcpy(buff + pointer, data, dlen);
	pointer += dlen;

	sendto(udp_sock_fd, buff, pointer, 0, (struct sockaddr*) &dest_addr,
			sizeof(dest_addr));
}

/**
 * This function adds the node to the triad ring
 */
void add_to_ring(triad_client *client) {
	printf("add_to_ring: adding client %s to the triad ring\n", client->name);
	//query to the client 1 in the ring.
	successor_q(client, client->client_1_triad_id, client->client_1_port);
}

/**
 * This function will reply the calculated nonce and local udp port back to the
 * manager once the node is added to the Triad ring.
 */
void reply_to_manager(triad_client *client, char *msg) {
	char buffer[MAXSIZE];
	int pid = getpid(); // to be sent to the manager.
	//calculate the new nonce
	printf("reply_to_manager: client=%s & udp_port=%d\n", client->name,
			client->local_udp_port);

	if (msg == NULL) {
		long new_nonce = client->nonce + pid;
		printf("do_client: computed nounce is: %ld\n", new_nonce);
		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "%ld %d\n%d\n", new_nonce, pid, client->local_udp_port);
		printf("do_client: local udp port number = %d\n",
				client->local_udp_port);
	} else {
		memcpy(buffer, msg, strlen(msg));
	}
	if (send(tcp_client_sock_fd, buffer, MAXSIZE - 1, 0) < 0) {
		perror("Error sending data to server");
	}
}

/**
 * This function handles the data received at the TCP socket
 */
void handle_tcp_receives(triad_client *client) {
	int stage;
	char buffer[MAXSIZE];
	memset(buffer, 0, sizeof(buffer));

	if (recv(tcp_client_sock_fd, buffer, MAXSIZE - 1, 0) < 0) {
		perror("Error in receiving data for server");
	}

	switch (atoi(buffer)) { // will covert the string till the new line character to integer
	case 1292:
		//tokenize the data received from the manager
		sscanf(buffer, "%d\n%d\n%d\n%s\n%d\n%s", &status, &stage,
				&client->nonce, client->name, &client->client_1_port,
				client->client_1_name);
		printf("do_client: data received from server: \n%d\n%d\n%s\n%d\n%s\n",
				stage, client->nonce, client->name, client->client_1_port,
				client->client_1_name);

		//prepare the client output file name
		sprintf(client_output_filename, "stage%d.%s.out", stage, client->name);
		printf("do_client: the client output fille name is=%s\n",
				client_output_filename);
		client_output_fd = open_file(client_output_filename, "w");

		//caculate the triad id of this client
		client->self_triad_id = get_triad_id(client->nonce, client->name);
		client->client_1_triad_id = get_triad_id(client->nonce,
				client->client_1_name);

		//The first client is its predecessor and successor otherwise client
		//should send messages to messages in the triad ring to find its location
		if (client->client_1_port == 0) {
			client->predecessor_triad_id = client->self_triad_id;
			client->successor_triad_id = client->self_triad_id;
			//after joining the triad ring log to output file
			fprintf(client_output_fd, "client %s created with hash 0x%x\n",
					client->name, client->self_triad_id);
			fflush(client_output_fd);

		} else {
			add_to_ring(client);
		}
		printf(
				"do_client: client=%s\tpredecessor_triad_id=0x%x\tsuccessor_triad_id0x=%x\n",
				client->name, client->predecessor_triad_id,
				client->successor_triad_id);

		// fetch the local port number and send back to the manager
		socklen_t addrlen = sizeof(udp_host_sock_addr);
		if (getsockname(udp_sock_fd, (struct sockaddr *) &udp_host_sock_addr,
				&addrlen) == 0) {
			client->local_udp_port = ntohs(udp_host_sock_addr.sin_port);
		}

		if (client->client_1_port == 0) { // only in the case of first client
			client->predecessor_udp_port = client->local_udp_port;
			client->successor_udp_port = client->local_udp_port;
			reply_to_manager(client, NULL);
		}

		break;
	case 557:
		memset(str, 0, sizeof(str));
		sscanf(buffer, "%d\n%s", &status, str);
		printf("handle_tcp_receives: client=%s\tstore %s\n", client->name, str);
		stores_q(client, client->successor_triad_id, client->successor_udp_port,
				str);
		break;
	case 630:
		printf("handle_tcp_receives: search \n");
		break;
	}

}

/**
 * This function handles the data received at the UDP socket
 */
void handle_udp_receives(triad_client *client) {
	unsigned int status = 0, data, pointer = 0, self_id = 0, successor_id = 0,
			successor_port = 0, result, flag;
	unsigned char buff[MAXSIZE];
	char* temp;
	struct sockaddr_in dest_addr;
	socklen_t dest_addr_len = sizeof(struct sockaddr);
	if (recvfrom(udp_sock_fd, buff, sizeof(buff), 0,
			(struct sockaddr*) &dest_addr, &dest_addr_len) < 0) {
		perror("Error receiving Triad messages\n");
	}

	memcpy(&status, buff, sizeof(status));
	status = ntohl(status);
	pointer += sizeof(status);

	//NOTE: all the odd numbers are the receivers and even number cases are requesters
	switch (status) {
	case 1:
		data = 0;
		memcpy(&data, buff + pointer, sizeof(data));
		data = ntohl(data);
		//log the transaction in the log file
		fprintf(client_output_fd, "successor-q received (0x%x)\n", data);
		fflush(client_output_fd);

		printf("handle_udp_receives: client=%s\tsuccessor-q received (0x%x)\n",
				client->name, data);
		//reply with successor-r message
		successor_r(dest_addr, client, client->self_triad_id,
				client->successor_triad_id, client->successor_udp_port);

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
		printf(
				"handle_udp_receives: client=%s\tsuccessor-r received (0x%x 0x%x %d)\n",
				client->name, self_id, successor_id, successor_port);

		//make a decision about location of this node in the triad ring
		result = decision_matrix(client->self_triad_id,
				client->client_1_triad_id, successor_id, self_id);

		/* 100 => position found update the predecessor and successor in the ring.
		 * 101 => self_id is to be added at the beginning
		 * 102 => self_id is to be added at the end
		 * 103 => only one element in the ring
		 */

		client->temp.tmp_successor_triad_id = successor_id;
		client->temp.tmp_successor_udp_port = successor_port;
		client->temp.tmp_predecessor_triad_id = self_id;
		client->temp.tmp_predecessor_udp_port = ntohs(dest_addr.sin_port);

		switch (result) {
		case 100:
		case 101:
		case 102:
		case 103:
			update_q(client, client->temp.tmp_successor_udp_port,
					client->temp.tmp_successor_triad_id, client->self_triad_id,
					client->local_udp_port, 0);
			update_q(client, client->temp.tmp_predecessor_udp_port,
					client->temp.tmp_predecessor_triad_id,
					client->self_triad_id, client->local_udp_port, 1);
			break;

		case 104:
			successor_q(client, successor_id, successor_port);
			break;
		}

		break;

	case 5:
		memcpy(&self_id, buff + pointer, sizeof(self_id));
		pointer += sizeof(self_id);
		memcpy(&result, buff + pointer, sizeof(result));
		pointer += sizeof(result); //result is reused to hold the data_hash value for this case

		self_id = ntohl(self_id);
		result = ntohl(result);

		//log the event
		fprintf(client_output_fd, "stores-q received (0x%x 0x%x)\n", self_id,
				result);
		fflush(client_output_fd);
		printf(
				"handle_udp_receives: client=%s\tstores-q received (0x%x 0x%x)\n",
				client->name, self_id, result);

		/*stores-r (ni; di; ri; rp; has) reply stating that ni's best estimate of the node that stores
		 di has id ri at port rp.*/
		if (result > client->predecessor_triad_id
				&& result <= client->self_triad_id) {
			flag = self_id == client->self_triad_id
					&& is_data_hash_present(client, result) ? 1 : 0;
			successor_id = client->self_triad_id;
			successor_port = client->local_udp_port;
		} else {
			flag = 0;
			successor_id = client->successor_triad_id;
			successor_port = client->successor_udp_port;
		}

		stores_r(dest_addr, client, self_id, result, successor_id,
				successor_port, flag);
		break;

	case 6:
		memcpy(&self_id, buff + pointer, sizeof(self_id));
		pointer += sizeof(self_id);
		memcpy(&result, buff + pointer, sizeof(result));
		pointer += sizeof(result);
		memcpy(&successor_id, buff + pointer, sizeof(successor_id));
		pointer += sizeof(successor_id);
		memcpy(&successor_port, buff + pointer, sizeof(successor_port));
		pointer += sizeof(successor_port);
		memcpy(&flag, buff + pointer, sizeof(flag));

		self_id = ntohl(self_id);
		result = ntohl(result);
		successor_id = ntohl(successor_id);
		successor_port = ntohl(successor_port);
		flag = ntohl(flag);

		//log the event
		fprintf(client_output_fd, "stores-r received (0x%x 0x%x 0x%x %d %d)\n",
				self_id, result, successor_id, successor_port, flag);
		fflush(client_output_fd);
		printf(
				"handle_udp_receives: client=%s\tstores-r received (0x%x 0x%x 0x%x %d %d)\n",
				client->name, self_id, result, successor_id, successor_port,
				flag);

		if (successor_id == self_id) {
			store_q(client, successor_id, successor_port, strlen(str), str);
		} else if (successor_id == client->client_1_triad_id) {
			//This means none of the node can have the string and the client 1
			//should store it. Hence it should log appropriate message in the log
			//file as well as should reply back to manager with "ok" message.
			add_data_to_client(client, str, strlen(str));
			fprintf(client_output_fd, "add %s with hash 0x%x to node 0x%x\n",
					str, get_triad_id(client->nonce, str), successor_id);
			fflush(client_output_fd);
			printf(
					"handle_udp_receives: client=%s\tadd %s with hash 0x%x to node 0x%x\n",
					client->name, str, get_triad_id(client->nonce, str),
					successor_id);

			memset(buff, 0, sizeof(buff));
			char *tmp = "ok\n";
			memcpy(buff, tmp, 2);
			reply_to_manager(client, (char*) buff);

		} else {
			stores_q(client, successor_id, successor_port, str);
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
		printf(
				"handle_udp_receives: client=%s\tupdate-q received (0x%x 0x%x %d %d)\n",
				client->name, self_id, successor_id, successor_port, flag);

		//reply back to the sender
		result = 1;
		//update the node properties
		if (flag) {
			client->successor_udp_port = successor_port;
			client->successor_triad_id = successor_id;
		} else {
			client->predecessor_udp_port = successor_port;
			client->predecessor_triad_id = successor_id;
		}

		update_r(dest_addr, client, result, client->self_triad_id, successor_id,
				successor_port, flag);

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

		if (result) {
			printf(
					"handle_udp_receives: client=%s\tupdate-r received (0x%x %d 0x%x %d %d)\n",
					client->name, self_id, result, successor_id, successor_port,
					flag);
			if (flag) {
				client->predecessor_udp_port =
						client->temp.tmp_predecessor_udp_port;
				client->predecessor_triad_id =
						client->temp.tmp_predecessor_triad_id;
			} else {
				client->successor_udp_port =
						client->temp.tmp_successor_udp_port;
				client->successor_triad_id =
						client->temp.tmp_successor_triad_id;
			}
		}

		//TODO this is not the correct place to put this function .. rework required!!!:(
		if (is_node_added_to_ring(client)) {
			reply_to_manager(client, NULL);
		}
		print_client_status(client);
		break;

	case 9:
		memcpy(&self_id, buff + pointer, sizeof(self_id));
		pointer += sizeof(self_id);
		memcpy(&result, buff + pointer, sizeof(result)); // result is used to store the string length
		pointer += sizeof(result);

		self_id = ntohl(self_id);
		result = ntohl(result);
		//data = ntohl(data);
		temp = (char*) malloc(sizeof(char) * (result + 1));
		if (temp == NULL) {
			//log the transaction in the log file
			fprintf(client_output_fd,
					"store-q: Failed to Allocate new memory for storing the data\n");
			fflush(client_output_fd);
			printf(
					"handle_udp_receives: store-q: Failed to Allocate new memory for storing the data for client=%s\n",
					client->name);
			store_r(dest_addr, client, self_id, 0, result, NULL);
			return;
		}
		memset(temp, 0, result + 1);
		memcpy(temp, buff + pointer, result); // data is used to store the string
		*(temp + result) = '\0'; //adding null character at the end of the string

		flag = add_data_to_client(client, temp, result);

		//log the transaction in the log file
		fprintf(client_output_fd, "store-q received (0x%x %d %s)\n", self_id,
				result, temp);
		fflush(client_output_fd);
		printf(
				"handle_udp_receives: client=%s\tstore-q received (0x%x %d %s)\n",
				client->name, self_id, result, temp);

		store_r(dest_addr, client, self_id, flag, result, temp);
		free(temp);
		break;

	case 10:
		memcpy(&self_id, buff + pointer, sizeof(self_id));
		pointer += sizeof(self_id);
		memcpy(&flag, buff + pointer, sizeof(flag));
		pointer += sizeof(flag);
		memcpy(&result, buff + pointer, sizeof(result)); //result is used to hold the string length information
		pointer += sizeof(result);

		self_id = ntohl(self_id);
		result = ntohl(result);
		flag = ntohl(flag);

		temp = (char*) malloc(result + 1);
		memset(temp, 0, result + 1);
		memcpy(temp, buff + pointer, result); //data is used to store actual string
		*(temp + result) = '\0';

		//log the transaction in the log file
		fprintf(client_output_fd, "store-r received (0x%x %d %d %s)\n", self_id,
				flag, result, temp);
		fprintf(client_output_fd, "add %s with hash 0x%x to node 0x%x\n", temp,
				get_triad_id(client->nonce, temp), self_id);
		fflush(client_output_fd);
		printf(
				"handle_udp_receives: client=%s\tstore-r received (0x%x %d %d %s)\n",
				client->name, self_id, flag, result, temp);
		free(temp);

		//reply back to manager that data has been set
		memset(buff, 0, sizeof(buff));
		char *tmp = "ok\n";
		memcpy(buff, tmp, 2);
		reply_to_manager(client, (char*) buff);
		break;
	}
}

/**
 * This function handles the I/O synchronously between multiple socket
 * file descriptors
 */
fd_set handle_io_synchronously(triad_client *client) {
	fd_set readfds;
	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 500000;
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
		NULL, &tv);
		if (rv == -1) {
			perror("select");
			break;
		}
		if (rv == 0) {
			//time out has occurred, this implies that the manager is either has
			//finished executing all instrcutions in the input file or has got
			//into some block
			kill_io = 1;
			break;
		} else {
			//if TCP socket receives data
			if (FD_ISSET(tcp_client_sock_fd, &readfds)) {
				//printf("handle_io_synchronously: Received TCP message\n");
				handle_tcp_receives(client);
			}
			//if UDP socket receives data
			if (FD_ISSET(udp_sock_fd, &readfds)) {
				printf("handle_io_synchronously: Received UDP message\n");
				handle_udp_receives(client);
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
	triad_client client;
	memset(&client, 0, sizeof(triad_client));

	int server_port = 0;
	char buffer[MAXSIZE];
	memset(buffer, 0, sizeof(buffer));
	char s[SEGSIZE];
	memset(s, 0, sizeof(s));

	// initialize the client application
	init_client(&client);

	//connect to the manager
	do {
		readshm(client_shm, s);
		printf("do_client: server port number read from shared memory is: %s\n",
				s);
		printf("do_client: trying to connect the server\n");
		server_port = atoi(s);
		client.manager_tcp_port = server_port;
		populate_sockaddr_in(&client_tcp_server, "localhost", server_port);
	} while (connect(tcp_client_sock_fd, (struct sockaddr *) &client_tcp_server,
			sizeof(client_tcp_server)) < 0);

	//ideal spot to put select() for synchronously hanlding the I/O
	fd_set readfds = handle_io_synchronously(&client);
	destroy_client(readfds);

}
