/*
 * This code "USC CSci551 Projects A and B FA2011" is
 * Copyright (C) 2011 by Xun Fan.
 * All rights reserved.
 *
 * This program is released ONLY for the purposes of Fall 2011 CSci551
 * students who wish to use it as part of their Project C assignment.
 * Use for another other purpose requires prior written approval by
 * Xun Fan.
 *
 * Use in CSci551 is permitted only provided that ALL copyright notices
 * are maintained and that this code is distinguished from new
 * (student-added) code as much as possible.  We new services to be
 * placed in separate (new) files as much as possible.  If you add
 * significant code to existing files, identify your new code with
 * comments.
 *
 * As per class assignments, use of any code OTHER than this provided
 * code requires explicit approval, ahead of time, by the professor.
 *
 */
 
// File name: 	comm.c
// Author: 		Xun Fan (xunfan@usc.edu)
// Date: 		2011.8
// Description: CSCI551 fall 2011 project b, communication module source file.

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int SendStreamData(int sock, char *buf, int length){
	int nRemain = length;
	int nSendbytes = 0;
	
	//keep sending until not remaining bytes
	while(nRemain > 0){
		if ((nSendbytes = send(sock, &buf[length-nRemain], nRemain, 0)) < 0){
		  return -1;  //on error	
		}else{
		  nRemain = nRemain-nSendbytes;  //calculate remaining bytes
		}
	}
	return 0;
}

int RecvStreamLineForSelect(int sock, char *buf, int bufsize){
        int		nRecvbytes = 0;   //bytes read per recv
	char		szRecvbyte[1];      //single char receive buffer
	int		recvdlength = 0;    //bytes received so far
	int		nRecvbufsize = bufsize;
	
	/*
	 * Keep calling recv until we have received bufsize number 
	 * of bytes OR we read a newline character OR we read 0 bytes.
	 */
	while(nRecvbufsize > 0)
	{
		//read single byte from socket
		if ((nRecvbytes = recv(sock, szRecvbyte, 1, 0)) < 0){
		  return -1;  //on error
		}
		if (nRecvbytes > 0){
			buf[recvdlength] = szRecvbyte[0];
			recvdlength += nRecvbytes;
			nRecvbufsize -= nRecvbytes;
			if (szRecvbyte[0] == '\n'){ //last byte	
			  break; //exit while
			}
		}
		if (nRecvbytes == 0)    // read on a closed socket
			break;
	}
	if (nRecvbufsize == 0 && buf[recvdlength-1] != '\n')
		return -2;
	else
		return recvdlength;
}
