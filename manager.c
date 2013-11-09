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


// File name: 	manager.c
// Author: 		Xun Fan (xunfan@usc.edu)
// Date: 		2011.8
// Description: CSCI551 fall 2011 project b, manager module source file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>


#include "manager.h"
#include "client.h"
#include "comm.h"
#include "sha1.h"




int nWrknum = 0;
char logfilename[256] = {0};

int sockMax;

int manager(void)
{

	int nSockmgr;
	struct sockaddr_in sinMgraddr, sinMgrcopy, sinWkraddr;
	socklen_t nsinLen;
	socklen_t nsinSize;
	int nPort = 0;
	int i;
	
	// print state.
	printf("projb manager: num_nodes: %d, nonce: %d\n", nClient, nNonce);
	

	// Create socket
	if ((nSockmgr = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		errexit("manager create socket.\n");
	}

	// initiate address
	sinMgraddr.sin_family = AF_INET;
	sinMgraddr.sin_port = htons(0);  //make random port number
	sinMgraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bzero(&(sinMgraddr.sin_zero), 8);

	// bind
	nsinLen = sizeof(struct sockaddr_in);

	if (bind(nSockmgr, (struct sockaddr *)&sinMgraddr, nsinLen) < 0){
		errexit("manager bind \n");
	}
	
	//get port number
	if (getsockname(nSockmgr, (struct sockaddr *)&sinMgrcopy, &nsinLen) < 0){
		errexit("manager getsockname\n");
	}
	
	//print port number
	nPort = ntohs(sinMgrcopy.sin_port);
	printf("projb manager: nPort is %d\n", nPort);



	// listen
	if (listen(nSockmgr, nClient) < 0){
		errexit("manager listen.\n");
	}


	// start running workers
	/*
	strcpy(logfilename, "stage1.manager.out");
	logfileinit(logfilename); // create the log file
	snprintf(logfilebuf, sizeof(logfilebuf), "manager port: %d\n", nPort);
	logfilewriteline(logfilename, logfilebuf, strlen(logfilebuf)); //write log
	*/

	for (i=0; i<nClient; i++)
	{
		if (fork() == 0){
			// this is the child process
			close(nSockmgr);
			client(nPort);
			exit(0);
		}
	}
	
	//i= nClient - 1;
	
	// use I/O multiplexing to communicate with clients
	
	fd_set readfds;
	fd_set sockset;
	struct timeval tv;
	//int got_sth_to_read = 0;/
	
	tv.tv_sec = SELECT_TIMEOUT; 
	tv.tv_usec = 0;
	
	sockMax = nSockmgr;
	FD_ZERO(&sockset);
	FD_SET(nSockmgr, &sockset);
	
	
	char	szNonce[16];
	char	szSendbuf[256];
	char	szRecvbuf[128];
	int	nBytestosend;
	int 	nRecvbytes;
	int 	nRecvbufsize = sizeof(szRecvbuf);
	
	// record worker counter
	//int	workercnt[FD_SETSIZE];
	//int	nWrkcnt = 1;
	
	// preparing nonce buffer, benefits every connection
	if (nNonce >= 0){
		snprintf(szNonce, sizeof(szNonce), "%d", nNonce);
	}
	
	pCName pcnPos = CNameHead;  // current pos in client list, for client creation
	int nCltcreated = 0;
	
	
	// first select round, for client creation.
	// this big while loop just sets up all the client connections.
	while(1){
		if(nCltcreated == nClient){
			break;
		}
		
		readfds = sockset;
		
		if (select(sockMax+1, &readfds, NULL, NULL, &tv) == -1) {
			errexit("manager select.\n");
		}
		
		for(i = 0; i <= sockMax; i++) {
			if (FD_ISSET(i, &readfds)){
				if (i == nSockmgr){		// new connection
					int newsock;
					nsinSize = sizeof(sinWkraddr); // for accept
					
					if ((newsock = accept(nSockmgr, (struct sockaddr *)&sinWkraddr, &nsinSize)) == -1){
						printf("projb manager: accept error!\n");
					}else{			// new connection, send nounce
						// send nonce and name and name and port
						if (pcnPos == CNameHead){    // first client
							snprintf(szSendbuf, sizeof(szSendbuf), "%s\n%s\n%d\n%s\n", szNonce, CNameHead->namestr, 0, CNameHead->namestr);
						}else{		//not the first client
							snprintf(szSendbuf, sizeof(szSendbuf), "%s\n%s\n%d\n%s\n", szNonce, pcnPos->namestr, CNameHead->udpport, CNameHead->namestr);
						}
						nBytestosend = strlen(szSendbuf);
						if (SendStreamData(newsock, szSendbuf, nBytestosend) < 0){
							printf("projb manager error: send nonce name etc error! Close socket!\n");
							close(newsock);
							continue;
						}
						
						// recv modified nonce and port number
						if ((nRecvbytes = RecvStreamLineForSelect(newsock, szRecvbuf, nRecvbufsize)) < 0){
							printf("projb manager error: receive client message error! Close socket!\n");
							close(newsock);
							continue;
						}// seems we don't need modified nonce, ignore it.
						
						szRecvbuf[nRecvbytes-1] = '\0';
						printf("projb manager: recv from client, %s\n", szRecvbuf);
						// get port number
						if ((nRecvbytes = RecvStreamLineForSelect(newsock, szRecvbuf, nRecvbufsize)) < 0){
							printf("projb manager error: receive client message error! Close socket!\n");
							close(newsock);
							continue;
						}else if (nRecvbytes == 0){ // should not happen
							printf("projb manager error: receive client message error! Close socket!\n");
							close(newsock);
							continue;
						}else{
							szRecvbuf[nRecvbytes]='\0'; 
							pcnPos->udpport = (unsigned short)atoi(szRecvbuf);
						}
						
						printf("manager: received client %s port %d\n", pcnPos->namestr, pcnPos->udpport);
						
						//workercnt[newsock] = nWrkcnt;
						//nWrkcnt++;
						
						// add socket to set
						FD_SET(newsock, &sockset);
						if (newsock > sockMax) {	// reset the maximum socket
							sockMax = newsock;
						}
						
						// store the UDP port and TCP sock.
						pcnPos->tcpsock = newsock;
						pcnPos = pcnPos->next;
						nCltcreated++;
					}
				}else{				// client socket. no actioin in this select loop
					
				}
			}
		}
	}
	
	int nJobleft = nMgrjob;
	printf("projb manager: nJohleft = %d\n", nJobleft);
	// Start store and other command, send the first store command
	if (nMgrjob != 0){
		pSText pstPos;
		pstPos = STextHead;
		
		snprintf(szSendbuf, sizeof(szSendbuf), "store\n%s\n", STextHead->txt);
		nBytestosend = strlen(szSendbuf);
		if(SendStreamData(CNameHead->tcpsock, szSendbuf, nBytestosend) < 0){
			printf("manager: send first store error!\n");
		}
		printf("manager: send one store job: %s\n", pstPos->txt);
		pstPos = pstPos->next;
		
		pmgrjob curjob = MgrjobHead->next;  // skip the first one
		pSearchT psrchPos = SearchTHead;
		pEndClnt pendclPos = EndClntHead;
		
		int nExit = 0;
		while(nJobleft){
			
			if (nExit==1)
				break;
						
			readfds = sockset;
			
			if (select(sockMax+1, &readfds, NULL, NULL, &tv) == -1) {
				errexit("manager select.\n");
			}
			
			for(i = 0; i <= sockMax; i++) {
				if (FD_ISSET(i, &readfds)){
					if (i == nSockmgr){   // not possible
						printf("projb manager: listen socket got an unexpected connection, ignore!\n");
					}else if(i == CNameHead->tcpsock){  // possibly first node finishes its job, good!
						// but check first
						if ((nRecvbytes = RecvStreamLineForSelect(i, szRecvbuf, nRecvbufsize)) < 0){
							errexit("projb manager error: receive first client message error!\n");
						}else if(nRecvbytes == 0){
							nExit = 1;
							break;
						}
						
						szRecvbuf[nRecvbytes-1] = '\0';
						if (strcmp(szRecvbuf, "ok") != 0){
							printf("projb manager exceptioin: recv unknown reply form first node: %s\n", szRecvbuf);
							nExit = 1;
							break;
						}
						
						nJobleft--;
						if (nJobleft == 0){
							break;
						}
						
						// assign next job
						if (curjob->jobtype == STRJOB){    // store
							snprintf(szSendbuf, sizeof(szSendbuf), "store\n%s\n", pstPos->txt);
							nBytestosend = strlen(szSendbuf);
							if(SendStreamData(CNameHead->tcpsock, szSendbuf, nBytestosend) < 0){
								printf("manager: send store error\n");
							}
							printf("manager: send one store job: %s\n", pstPos->txt);
							pstPos = pstPos->next;
						}else if(curjob->jobtype == SCHJOB){   // search
							
							snprintf(szSendbuf, sizeof(szSendbuf), "search\n%s\n", psrchPos->txt);
							nBytestosend = strlen(szSendbuf);
							if(SendStreamData(CNameHead->tcpsock, szSendbuf, nBytestosend) < 0){
								printf("manager: send search error\n");
							}
							printf("manager: send one search job: %s\n", psrchPos->txt);
							psrchPos = psrchPos->next;
						}else if(curjob->jobtype == ENDJOB){   // end_client
							// first find the client sock
							//pendclPos
							pCName tempcn = CNameHead;

							int k;
							for(k=0; k<nClient; k++){
								if (strcmp(tempcn->namestr, pendclPos->namestr) == 0)
									break;

								tempcn = tempcn->next;
							}
							if (tempcn == NULL){
								printf("manager: error, node to end is not one of the ring\n");
								nExit = 1;
								break;
							}
							snprintf(szSendbuf, sizeof(szSendbuf), "end_client\n%s\n", pendclPos->namestr);
							nBytestosend = strlen(szSendbuf);
							if(SendStreamData(tempcn->tcpsock, szSendbuf, nBytestosend) < 0){
								printf("manager: send end_client error\n");
							}
							printf("manager: send one end_client job: %s\n", pendclPos->namestr);

							pendclPos = pendclPos->next;
							
						}
						
						curjob = curjob->next;
						
					}else{ // end client
						if ((nRecvbytes = RecvStreamLineForSelect(i, szRecvbuf, nRecvbufsize)) < 0){
							errexit("projb manager error: receive first client message error!\n");
						}else if(nRecvbytes == 0){
							nExit = 1;
							break;
						}
						
						szRecvbuf[nRecvbytes-1] = '\0';
						if (strcmp(szRecvbuf, "ok") != 0){
							printf("projb manager exceptioin: recv unknown reply form first node: %s\n", szRecvbuf);
							nExit = 1;
							break;
						}
						nJobleft--;
						if (nJobleft == 0){
							break;
						}

						// assign next job
						if (curjob->jobtype == STRJOB){    // store
							snprintf(szSendbuf, sizeof(szSendbuf), "store\n%s\n", pstPos->txt);
							nBytestosend = strlen(szSendbuf);
							if(SendStreamData(CNameHead->tcpsock, szSendbuf, nBytestosend) < 0){
								printf("manager: send store error\n");
							}
							printf("manager: send one store job: %s\n", pstPos->txt);
							pstPos = pstPos->next;
						}else if(curjob->jobtype == SCHJOB){   // search
							
							snprintf(szSendbuf, sizeof(szSendbuf), "search\n%s\n", psrchPos->txt);
							nBytestosend = strlen(szSendbuf);
							if(SendStreamData(CNameHead->tcpsock, szSendbuf, nBytestosend) < 0){
								printf("manager: send search error\n");
							}
							printf("manager: send one search job: %s\n", psrchPos->txt);
							psrchPos = psrchPos->next;
						}else if(curjob->jobtype == ENDJOB){   // end_client
							// first find the client sock
							//pendclPos
							pCName tempcn = CNameHead;

							int k;
							for(k=0; k<nClient; k++){
								if (strcmp(tempcn->namestr, pendclPos->namestr) == 0)
									break;

								tempcn = tempcn->next;
							}
							if (tempcn == NULL){
								printf("manager: error, node to end is not one of the ring\n");
								nExit = 1;
								break;
							}
							snprintf(szSendbuf, sizeof(szSendbuf), "end_client\n%s\n", pendclPos->namestr);
							nBytestosend = strlen(szSendbuf);
							if(SendStreamData(tempcn->tcpsock, szSendbuf, nBytestosend) < 0){
								printf("manager: send end_client error\n");
							}
							printf("manager: send one end_client job: %s\n", pendclPos->namestr);

							pendclPos = pendclPos->next;
							
						}
						
						curjob = curjob->next;
					}
				}
			}
			
			if (nJobleft == 0)
				break;
		}
	}

	// ask cliens to exit;
	snprintf(szSendbuf, sizeof(szSendbuf), "exit!\n");
	nBytestosend = strlen(szSendbuf);
	pcnPos = CNameHead;
	for (i=0; i<nClient; i++){
		if(SendStreamData(pcnPos->tcpsock, szSendbuf, nBytestosend) < 0){
			printf("manager: send exit error, client %s\n", pcnPos->namestr);
		}
		close(pcnPos->tcpsock);
	//	printf("manager close: %s with sock %d\n", pcnPos->namestr, pcnPos->tcpsock);
		pcnPos = pcnPos->next;
	}
	close(nSockmgr);
	return 0;
}

