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

// File name: 	projb.h
// Author: 		Xun Fan (xunfan@usc.edu)
// Date: 		2011.8
// Description: CSCI551 fall 2011 project b, main module header file.

#ifndef _PROJA_H
#define _PROJA_H

#define GENERAL_SIZE 128
#define CONFIG_FILE_BUF_SIZE 128
#define FILE_NAME_SIZE 256
#define MAX_CLIENT 128
#define MAX_CLIENT_NAME_SIZE 96
#define MAX_STORE_TEXT_SIZE 96

extern int nClient;
extern unsigned int nNonce;
extern int nStage;


typedef struct ClientName  
{
	char namestr[MAX_CLIENT_NAME_SIZE];
	int tcpsock;    // reserved
	unsigned short udpport;
	struct ClientName *next;
}CNAME, *pCName;
extern pCName CNameHead;
extern pCName CNameTail;

typedef struct StoreText
{
	char txt[MAX_STORE_TEXT_SIZE];
	int flag;	//reserved
	struct StoreText *next;
}STEXT, *pSText;
extern pSText STextHead;
extern pSText STextTail;
extern int nSText;

typedef struct SearchText
{
	char txt[MAX_STORE_TEXT_SIZE];
	struct SearchText *next;
}SEARCHT, *pSearchT;
extern pSearchT SearchTHead;
extern pSearchT SearchTTail;
extern int nSearchT;

typedef struct MgrJobList{
	int	jobtype;
	struct MgrJobList *next;
}MGRJOB, *pmgrjob;
extern pmgrjob MgrjobHead;
extern pmgrjob MgrjobTail;
extern int nMgrjob;

typedef struct EndClntNode{
	char namestr[MAX_CLIENT_NAME_SIZE];
	struct EndClntNode *next;
}ENDCLNT, *pEndClnt;
extern pEndClnt EndClntHead;
extern pEndClnt EndClntTail;
extern int nEndClnt;

#define STRJOB 1   // store
#define SCHJOB 2   // search
#define ENDJOB 3   // end_client

void errexit(char *msg);
void AddClientNameNode(char *name);
void AddStoreTextNode(char *text);
void AddSearchTextNode(char *str);
void AddMgrJobNode(int job);
void AddEndClntNode(char *str);
#endif
