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


// File name: 	projb.c
// Author: 		Xun Fan (xunfan@usc.edu)
// Date: 		2011.8
// Description: CSCI551 fall 2011 project b, main module source file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "projb.h"
#include "manager.h"
#include "client.h"

int nClient = -1;
unsigned int nNonce = -1;
int nStage = 0;

pCName CNameHead = NULL; // client name list head
pCName CNameTail = NULL; // client name list tail

pSText STextHead = NULL; // store text list head
pSText STextTail = NULL; // store text list tail
int nSText = 0; // number of store texts

pSearchT SearchTHead = NULL; // search text list head
pSearchT SearchTTail = NULL;
int nSearchT = 0; // search text list size

pmgrjob MgrjobHead = NULL;
pmgrjob MgrjobTail = NULL;
int nMgrjob = 0;

pEndClnt EndClntHead = NULL;
pEndClnt EndClntTail = NULL;
int nEndClnt = 0;

int main(int argc, char *argv[])
{
	char *usage = "Usage: projb [configuration file]\n";
	char sClientname[MAX_CLIENT_NAME_SIZE] = {0};
	char sText[MAX_STORE_TEXT_SIZE] = {0};
 	char sKeyword[64] = {0};

	//check arguments
	if (argc == 1)
	{
		printf("no configuration file!\n\n%s", usage);
		exit(1);
	}
	else if(argc != 2){
		printf("%s", usage);
		exit(1);
	}

	FILE *fp;
	char confile_buf[CONFIG_FILE_BUF_SIZE] = {0};
	char cFilename[FILE_NAME_SIZE] = {0};
	
	// get configuration file path
	if (argv[1][0] != '/'){    // relative path
		snprintf(cFilename, sizeof(cFilename), "./%s", argv[1]);
	}else{							// absolute path
		if (strlen(argv[1]) > FILE_NAME_SIZE ){
			errexit("input path too long.\n");
		}
		strncpy(cFilename, argv[1], FILE_NAME_SIZE);
	}
	
	
	// Open configuration file
	if ((fp = fopen(cFilename, "r")) == NULL)
	{
		printf("open configuration file error. %s\n", cFilename);
	}

	//read config file
	while (!feof(fp))
	{
		// read line
		memset(confile_buf, 0, sizeof(confile_buf));
		fgets(confile_buf, CONFIG_FILE_BUF_SIZE,fp);

		if (confile_buf[0] == '#') //comment line
			continue;
		
		if (strlen(confile_buf) == 0)
			continue;
		
		if (sscanf(confile_buf, "%s", sKeyword) != 1)
			continue;
		
		if (strcmp(sKeyword, "stage") == 0){
			if (sscanf(confile_buf, "%s %d", sKeyword, &nStage) != 2)
				errexit("config file wrong stage statement.\n");
		}
		
		if (strcmp(sKeyword, "nonce") == 0){
			if (sscanf(confile_buf, "%s %d", sKeyword, &nNonce) != 2)
				errexit("config file wrong nonce statement.\n");
		}
		
		if (strcmp(sKeyword, "start_client") == 0){
			if (sscanf(confile_buf, "%s %s", sKeyword, sClientname) != 2)
				errexit("config file wrong start_client statement.\n");
			
			// add client name to list
			AddClientNameNode(sClientname);
		}
		
		if (strcmp(sKeyword, "store") == 0){
			if (sscanf(confile_buf, "%s %s", sKeyword, sText) != 2)
				errexit("config file wrong store statement.\n");
			
			// add client name to list
			AddStoreTextNode(sText);
			AddMgrJobNode(STRJOB);
		}
		
		if (strcmp(sKeyword, "search") == 0){
			if (sscanf(confile_buf, "%s %s", sKeyword, sText) != 2)
				errexit("config file wrong store statement.\n");
			
			// add client name to list
			AddSearchTextNode(sText);
			AddMgrJobNode(SCHJOB);
		}

		if (strcmp(sKeyword, "end_client") == 0){
			if (sscanf(confile_buf, "%s %s", sKeyword, sText) != 2)
				errexit("config file wrong store statement.\n");
			
			// add client name to list
			AddEndClntNode(sText);
			AddMgrJobNode(ENDJOB);
		}
	}

	// Check if configuration parameter read successfully
	// projb only accept stage 2, 3, 4, 5
	if (nClient <= 0 || nNonce < 0 || nStage < 2 || nStage > 5){
		errexit("configuration file parameters wrong!\n");
	}

	//start task
	
	if (manager() < 0){
		return -1;
	}

	return 0;
}

void errexit(char *msg)
{
       printf("projb error: %s", msg);
       exit(1);
}

void AddClientNameNode(char *name){
	
	if (CNameHead == NULL){ // first node
		CNameHead = (pCName)malloc(sizeof(CNAME));
		if (CNameHead == NULL)
			errexit("malloc fail!\n");
			
		strncpy(CNameHead->namestr, name, MAX_CLIENT_NAME_SIZE);
		CNameHead->next = NULL;
		CNameTail = CNameHead;

		//printf("add client name: %s\n", CNameHead->namestr);
		nClient = 1; //update number of clients to 1
	} else {
		pCName newClient = (pCName)malloc(sizeof(CNAME));
		if (newClient == NULL)
			errexit("malloc fail!\n");
			
		strncpy(newClient->namestr, name, MAX_CLIENT_NAME_SIZE);
		newClient->next = NULL;
		CNameTail->next = newClient;
		CNameTail = newClient;
		//printf("add client name: %s\n", newClient->namestr);
		nClient++;
	}
}

void AddStoreTextNode(char *text){
	
	if (STextHead == NULL){ // first node
		STextHead = (pSText)malloc(sizeof(STEXT));
		if (STextHead == NULL)
			errexit("malloc fail!\n");
			
		strncpy(STextHead->txt, text, MAX_STORE_TEXT_SIZE);
		STextHead->next = NULL;
		STextTail = STextHead;
		nSText = 1;
	} else {
		pSText newText = (pSText)malloc(sizeof(STEXT));
		if (newText == NULL)
			errexit("malloc fail!\n");
			
		strncpy(newText->txt, text, MAX_STORE_TEXT_SIZE);
		newText->next = NULL;
		STextTail->next = newText;
		STextTail = newText;
		
		nSText++;
	}
}

void AddSearchTextNode(char *str){
	
	if (SearchTHead == NULL){ // first node
		SearchTHead = (pSearchT)malloc(sizeof(SEARCHT));
		if (SearchTHead == NULL)
			errexit("malloc fail!\n");
			
		strncpy(SearchTHead->txt, str, MAX_STORE_TEXT_SIZE);
		SearchTHead->next = NULL;
		SearchTTail = SearchTHead;
		nSearchT = 1;
	} else {
		pSearchT newSearch = (pSearchT)malloc(sizeof(SEARCHT));
		if (newSearch == NULL)
			errexit("malloc fail!\n");
			
		strncpy(newSearch->txt, str, MAX_STORE_TEXT_SIZE);
		newSearch->next = NULL;
		SearchTTail->next = newSearch;
		SearchTTail = newSearch;
		
		nSearchT++;
	}
	
}

void AddEndClntNode(char *str){
	pEndClnt temp;
	
	if (EndClntHead == NULL){ // first node
		EndClntHead = (pEndClnt)malloc(sizeof(ENDCLNT));
		if (EndClntHead == NULL)
			errexit("malloc fail!\n");
			
		strncpy(EndClntHead->namestr, str, MAX_CLIENT_NAME_SIZE);
		EndClntHead->next = NULL;
		EndClntTail = EndClntHead;
		nEndClnt = 1;
	} else {
		temp = (pEndClnt)malloc(sizeof(ENDCLNT));
		if (temp == NULL)
			errexit("malloc fail!\n");
			
		strncpy(temp->namestr, str, MAX_CLIENT_NAME_SIZE);
		temp->next = NULL;
		EndClntTail->next = temp;
		EndClntTail = temp;
		
		nEndClnt++;
	}
	
}


void AddMgrJobNode(int job){
	pmgrjob temp;
	if (MgrjobHead == NULL){
		MgrjobHead = (pmgrjob)malloc(sizeof(MGRJOB));
		if (MgrjobHead == NULL)
			errexit("malloc fail!\n");
		
		MgrjobHead->jobtype = job;
		MgrjobHead->next = NULL;
		MgrjobTail = MgrjobHead;
		nMgrjob = 1;
	}else{
		temp = (pmgrjob)malloc(sizeof(MGRJOB));
		if (temp == NULL)
			errexit("malloc fail!\n");
		
		temp->jobtype = job;
		temp->next = NULL;
		MgrjobTail->next = temp;
		MgrjobTail = temp;
		nMgrjob++;
	}
}
