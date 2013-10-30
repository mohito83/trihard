/*
 * client.h
 *
 *  Created on: Oct 24, 2013
 *      Author: csci551
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include "file_io_op.h"
#include "sock_op.h"
#include "my402list.h"
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>

#define SEGSIZE 10
#define MAXSIZE 256
#define BACKLOG_QUEUE 100

typedef struct temp_data {
	unsigned int tmp_predecessor_triad_id;
	unsigned int tmp_successor_triad_id;

	unsigned int tmp_predecessor_udp_port;
	unsigned int tmp_successor_udp_port;
} temp_client_data;

typedef struct client {
	char name[80];
	char client_1_name[80];
	My402List data_list;
	unsigned int client_1_port;
	unsigned int nonce;
	//unsigned int data_hash;	// hash key for the data

	unsigned int client_1_triad_id;

	unsigned int self_triad_id;
	unsigned int predecessor_triad_id;
	unsigned int successor_triad_id;

	unsigned int local_udp_port;
	unsigned int predecessor_udp_port;
	unsigned int successor_udp_port;

	unsigned int manager_tcp_port;

	//temporary data items to be
	temp_client_data temp;
} triad_client;

void do_client();

void print_client_status(triad_client *client);

int is_data_hash_present(triad_client *client, unsigned int hash_to_comapre);

#endif /* CLIENT_H_ */
