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


// File name:   client.h
// Author:    Xun Fan (xunfan@usc.edu)
// Date:    2011.8
// Description: CSCI551 fall 2011 project b, client module header file.

#ifndef _CLIENT_H
#define _CLIENT_H

// Triadnode
typedef struct TriadNode{
  unsigned int id;
  unsigned short port;
}TNode;

// message type
#define SUCCQ 1
#define SUCCR 2
#define PREDQ 3
#define PREDR 4
#define CLSTQ 5
#define CLSTR 6
#define UPDTQ 7
#define UPDTR 8
#define STORQ 9
#define STORR 10
#define LEAVQ 21
#define LEAVR 22
#define NXTDQ 23
#define NXTDR 24

#define MAX_TEXT_SIZE 96

#define HASHMAX 0xFFFFFFFF

#define SENTFLAG 1
#define RECVFLAG 2

#define FTLEN 33

// Triad messages
typedef struct ngbrquerymsg{  // for successor query and predecessor query
  int msgid;
  unsigned int  ni;
}NGQM, *pngqm;

typedef struct ngbrreplymsg{  // for successor reply and predecessor reply
  int msgid;
  unsigned int  ni;
  unsigned int  si;
  int sp;
}NGRM, *pngrm;

typedef struct clstquerymsg{
  int   msgid;
  unsigned int  ni;
  unsigned int  di;
}CLQM, *pclqm;

typedef struct clstreplymsg{
  int msgid;
  unsigned int  ni;
  unsigned int  di;
  unsigned int  ri;
  int rp;
  int nhas;
}CLRM, *pclrm;


typedef struct updtquerymsg{
  int msgid;
  unsigned int  ni;
  unsigned int  si;
  int sp;
  int i;
}UPQM, *pupqm;

typedef struct storquerymsg{
  int   msgid;
  unsigned int  ni;
  int sl;
}STQM, *pstqm;

typedef struct storreplymsg{
  int msgid;
  unsigned int  ni;
  int r;
  int sl;
}STRM, *pstrm;

typedef struct updtreplymsg{
  int msgid;
  unsigned int  ni;
  int       r;
  unsigned int  si;
  int sp;
  int i;
}UPRM, *puprm;

typedef struct leavquerymsg{
  int msgid;
  unsigned int ni;
  unsigned int di;
}LEQM, *pleqm;

typedef struct leavreplymsg{
  int msgid;
  unsigned int ni;
}LERM, *plerm;

typedef struct nxtdquerymsg{
  int msgid;
  unsigned int di;
  unsigned int id;
}NXQM, *pnxqm;

typedef struct nxtdreplymsg{
  int msgid;
  unsigned int di;
  unsigned int qid;
  unsigned int rid;
  int sl;
}NXRM, *pnxrm;


typedef struct ClientStore
{
  char txt[MAX_TEXT_SIZE];
  unsigned int id;  // hash id of the str
  struct ClientStore *next;
}CSTORE, *pCStore;

// finger table structure, one direction ring
typedef struct FingerTableNode
{
  unsigned int  start;
  unsigned int  end;
  TNode   node;
}FTNODE;




int client(int mgrport);
int GetInitInfo(int sock, char *selfname, char *firstnode, unsigned int *nonce, unsigned short *port);
int JoinRing(int sock);
int FindNeighbor(int sock, int msgtype, TNode na, TNode *pnb);
void LogTyiadMsg(int type, int sorr, char *buf);
int UpdateNeighbor(int sock, TNode *chgpreNode, TNode *chgsucNode);
int HandleUdpMessage(int sock);
int HandleStoreMsg(int sock, char *str);
int HandleSearchMsg(int sock, char *str);
int SearchClientStore(unsigned int id, char *str);
int FindClosest(int sock, int msgt, unsigned int targetid, TNode na, TNode *pnb);
void InitFingerTableSelf();
int InitFingerTable(int sock);
int FindSuccWithFT(int sock, unsigned int id, TNode *retnode);
int UpdateOthers(int sock);

int UpdateFingerTable(int sock, TNode tn, TNode sn, int idx);
int UpdateMyFingerTable(int sock, TNode s, int idx);
void UpdateMyFingerTableInit();
void ClosestPrecedingFinger(unsigned int id, TNode *tn);

// functions that used in stage 4, 5
int JoinRingWithFingerTable(int sock);
int HandleEndClient(int sock);
int LeaveUpdateNeighbor(int sock, TNode *chgpreNode, TNode *chgsucNode);

void AddClientStore(unsigned int id, char *str);

// debug only
void LogFingerTable();
#endif
