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
#include <sys/fcntl.h>

#define SEGSIZE 10
#define MAXSIZE 256
#define BACKLOG_QUEUE 100
#define NAME_LENGTH     80
#define FINGER_TABLE_SIZE 32

/****************************************************
 *************Finger Table Entry**********************
 *****************************************************/
typedef struct finger_table_entry {
	unsigned int start_int;
	unsigned int end_int;
	unsigned int successor_id;
	unsigned int successor_port;
} finger_entry_t;

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
	finger_entry_t f_entry_t[FINGER_TABLE_SIZE];
} triad_client;

typedef struct my_node {
	unsigned int triad_id;
	unsigned int port_no;
} node;

void do_client();

void print_client_status(triad_client *client);

int is_data_hash_present(triad_client *client, unsigned int hash_to_comapre);

#endif /* CLIENT_H_ */
