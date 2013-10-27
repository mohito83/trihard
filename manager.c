/*
 * manager.c
 *
 *  Created on: Sep 28, 2013
 *      Author: csci551
 */

#include "file_io_op.h"
#include "sock_op.h"
#include "client.h"
#include "sha1.h"

/**
 * a[0]: stage #
 * a[1]: # of clients
 * a[2]: nounce
 */
//long a[3] = { 0, 0, 0 };
long nonce = 0;
int stage, status, client1_port_no = 0;
const char* output_filename = "stage2.manager.out";
char client1_name[80];
/*
 * For shared memory
 */
int shmid, cntr;
char *segptr;

/**
 * For TCP socket prep
 */
struct sockaddr_in client_tcp_server, tmp, tcp_client;
int tcp_serv_sock_fd;

FILE* out_file_stream;

/**
 * This function will initialize the connection and stuff
 */
void init_process() {
	//4. Create shared memory area with the child processes.
	// REUSED CODE :- http://www.tldp.org/LDP/lpg/node81.html
	//--START
	key_t key;

	/* Create unique key via call to ftok() */
	key = ftok("./test", 'S');

	if ((shmid = shmget(key, SEGSIZE, IPC_CREAT | 0666)) < 0) {
		perror("shmget");
		exit(1);
	}

	/* Attach (map) the shared memory segment into the current process */
	if ((segptr = (char *) shmat(shmid, NULL, 0)) == (char *) -1) {
		perror("shmat");
		exit(1);
	}

	//--END

	//open file in output stream
	out_file_stream = open_file(output_filename, "w");
	//fprintf(out_file_stream, "stage 1\n");

	//3. set up TCP server at manager
	tcp_serv_sock_fd = create_tcp_socket();
	populate_sockaddr_in(&client_tcp_server, "localhost", 0);
	if (bind_address(tcp_serv_sock_fd, client_tcp_server) < 0) {
		perror("Error biding the address to socket. Exiting!!");
		exit(0);
	}

	//get the port number information
	socklen_t size = sizeof(tmp);
	if (getsockname(tcp_serv_sock_fd, (struct sockaddr *) &tmp, &size) < 0) {
		perror("Error getting port number information!!");
		exit(0);
	} else {
		fprintf(out_file_stream, "manager port: %u\n", ntohs(tmp.sin_port));
		fflush(out_file_stream);
	}

	//listen for incomming connections
	listen(tcp_serv_sock_fd, BACKLOG_QUEUE);

	//5. Put manager's port # in shared memory so that child processes and use it to connect the manager (server) socket
	//writeshm(segptr, temp);
	sprintf(segptr, "%u", ntohs(tmp.sin_port));
	printf("init_process: data set in shared memory is: %s\n", segptr);
}

/**
 * This function calculate the sum of the values of the characters of the first
 * token. This value is used by the switch case to identify the correct case
 */
int sum(char *str) {
	int sum = 0, i = 0;
	while (str[i] != '\0') {
		sum += str[i];
		i++;
	}
	return sum;
}

/**
 * This function handles the TCP communication with the clients
 * @stage: stage of the project
 * @nonce : nonce read from the input file
 * @client: client name
 * @port_no: "0" or port number of the first client
 * @second_name: same as client_name for first client else first client's client_name for other nodes
 */
void handle_client(int stage, long nonce, char* client_name, char* port_no,
		char* second_name) {
	char temp[MAXSIZE];
	int client_sock_fd;
	long mod_nonce = 0;
	socklen_t tcp_client_addr_len = sizeof(tcp_client);

	printf("handle_client:: waiting for connection!!\n");
	client_sock_fd = accept(tcp_serv_sock_fd, (struct sockaddr *) &tcp_client,
			&tcp_client_addr_len);
	if (client_sock_fd < 0)
		perror("ERROR on accept");

	//6. Do data transfer and log it in the log file.
	fprintf(out_file_stream, "client %s port: %u\n", client_name,
			tcp_client.sin_port);

	//prepare the payload for the client with stage,nonce,name,port no, second
	//name sepearted by '\n'
	char tmp[15];
	memset(tmp, 0, sizeof(tmp));
	memset(temp, 0, sizeof(temp));

	sprintf(tmp, "%d", status);
	strcat(temp, tmp);
	strcat(temp, "\n");

	memset(tmp, 0, sizeof(tmp));
	sprintf(tmp, "%d", stage);
	strcat(temp, tmp);
	strcat(temp, "\n");

	memset(tmp, 0, sizeof(tmp));
	sprintf(tmp, "%ld", nonce);
	strcat(temp, tmp);
	strcat(temp, "\n");

	strcat(temp, client_name);
	strcat(temp, "\n");

	strcat(temp, port_no);
	strcat(temp, "\n");

	strcat(temp, second_name);
	strcat(temp, "\n");
	// --END

	if (send(client_sock_fd, temp, sizeof(temp), 0) < 0)
		perror("Error in sending stage to client");

	printf("handle_client: payload for client is: %s\n", temp);

	//wait to receive reply from the clients
	memset(temp, 0, sizeof(temp));
	if (recv(client_sock_fd, temp, sizeof(temp), 0) < 0)
		perror("Error in receiving data from client");

	printf("handle_client: data received at the server: %s\n", temp);

	//parse the response
	int pid;
	sscanf(temp, "%ld %d\n%d", &mod_nonce, &pid, &client1_port_no);
	printf("handle_client: client1_port_no received=%d\n",client1_port_no);

	//client 1 says: 23504148 24713
	fprintf(out_file_stream, "client %s says: %ld %d\n", client_name, mod_nonce, pid);
	fflush(out_file_stream);
	//close(client_sock_fd);
	//TODO decide later when to close the client socket
}

/**
 * This function reads the input file and perform operation as per the
 * instruction given on each line.
 */
void read_input_file(char *filename) {
	int child;
	char buff[256], first[15], second[80], port_no[6], second_name[80];
	memset(buff, 0, sizeof(buff));
	memset(first, 0, sizeof(first));
	memset(second, 0, sizeof(second));

	//open file
	FILE* fp = open_file(filename, "r");
	if (fp == NULL) {
		perror("Error opening the file. Exiting!!");
		exit(0);
	}

	while (read_line(fp, buff, sizeof(buff))) {
		//printf("Data read from file Line #%d: %s\n", i, buff);
		if (buff[0] == '#')
			continue;

		//parse each line and perform appropriate operation
		sscanf(buff, "%s %s", first, second);
		/*
		 * Key for switch case:
		 * if sum(first)== 532	=> stage
		 * if sum(first)== 531	=> nonce
		 * if sum(first)== 1292	=> start_client
		 * if sum(first)== 557	=> store
		 * if sum(first)== 630	=> search
		 */
		switch (status = sum(first)) {
		case 532:
			stage = atoi(second);
			break;
		case 531:
			nonce = atol(second);
			break;
		case 1292:
			printf("read_input_file: start_client %s\n", second);
			if ((child = fork()) == 0) {
				do_client();
				exit(0);
			} else {
				//non-blocking wait
				waitpid(child, 0, WNOHANG);
				if (client1_port_no == 0) {
					sprintf(port_no, "%d", 0);
					memcpy(second_name, second, sizeof(second));
					memcpy(client1_name, second, sizeof(second));
				} else {
					sprintf(port_no, "%d", client1_port_no);
					memcpy (second_name,client1_name,sizeof(client1_name));
				}
				handle_client(stage, nonce, second, port_no, second_name);
			}
			break;
		case 557:
			printf("read_input_file: sum=%d\tstore %s\n", sum(first), second);
			break;
		case 630:
			printf("read_input_file: sum=%d\tsearch %s\n", sum(first), second);
			break;
		}

		memset(first, 0, sizeof(first));
		memset(second, 0, sizeof(second));
	}

	//TODO incoroprate some check on the input parameters defined in the file
	/*if(!(a[0]&&a[1]&&a[2])){
	 printf("Malformed input parameter file. Exiting!!\n");
	 exit(0);
	 }*/

	//close the file stream
	close_file(fp);
}

void writeshm(char *segptr, char *text) {
	strcpy(segptr, text);
//printf("Done...\n");
}

void destroy_process() {
	//removing shared memory area
	shmctl(shmid, IPC_RMID, 0);

	//close server socket before exiting the application
	close(tcp_serv_sock_fd);
}

int main(int argc, char *argv[]) {
	//check for correct usage
	if (argc < 2) {
		printf("The correct usage is ./proja <filename>\n");
		perror("Incorrect usage of program!! exiting!!\n");
		return -1;
	}

	init_process();

	char *filename = argv[1];

	//1. read the input parameter file
	read_input_file(filename);

	destroy_process();
	return 0;
}
