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
#include <sys/shm.h>
#include <sys/ipc.h>

#define SEGSIZE 10
#define MAXSIZE 256
#define BACKLOG_QUEUE 100

void do_client();

#endif /* CLIENT_H_ */
