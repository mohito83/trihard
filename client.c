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

// File name:   client.c
// Author:    Xun Fan (xunfan@usc.edu)
// Date:    2011.8
// Description: CSCI551 Fall 2011 project b, client module source file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "projb.h"
#include "client.h"
#include "sha1.h"
#include "comm.h"
#include "log.h"
#include "ring.h"

unsigned int    HashID;// my hash id
unsigned int    HashIDfirst;   // first node hash id
char            Myname[MAX_CLIENT_NAME_SIZE] = {0};  // Triad string name of self
unsigned short  MyUDPPort;  // my udp port
unsigned short  FirstNodePort; // first created node port
char            FirstNodeName[MAX_CLIENT_NAME_SIZE] = {0};
unsigned int    nMgrNonce = 0;

pCStore CStoreHead = NULL;  // client stored string stack head
int nCStore = 0;

FTNODE  MyFT[FTLEN];  // finger table
TNode succ; // successor node
TNode pred; // predecessor node


char logfilename[256];

//
// XXX Read and understand this function first!
// 
// This instantiates the client and contains the main select loop for
// handling all the commands sent to the client from other clients
// or the manager.
//
// The client also exits gracefully when the manager process exits.
//
// XXX There are some debugging messages that might be helpful in
// understanding the code; most of these have been commented out.
// Do a search through this file for "debug".
//
int client(int mgrport)
{
  int nPid;
  int nMgrport;
  
  // get manager port number
  nMgrport = mgrport;
    
  // get pid
  nPid = getpid();

  //printf("worker pid: %d manager port:%d\n", nPid, nMgrport);
  
  int   nSockwkr, udpSock;//,nBytesnum
  socklen_t nSinsize;
  struct  sockaddr_in sinWkraddr, sinWkraddrudp, sinWkrcopy;
  char  szRecvbuf[128];
  char  szSendbuf[128];
  int   nRecvbyte;
  int nSendlen;//, nRemainlen;//, nSent;
  //int   nRecvbufsize, nWrtpos;
  int   nSum;
  //char  szWritebuf[256];
  
  
  fd_set  readset;
  fd_set  allset;
  struct timeval tv;

  // Create socket
  if ((nSockwkr = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    printf("projb worker %d error: create socket error! Exit!\n", nPid);
    return -1;
  }

  // initiate address
  sinWkraddr.sin_family = AF_INET;
  sinWkraddr.sin_port = htons(nMgrport);  //manager port
  
  // connect to local address
  sinWkraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  
  bzero(&(sinWkraddr.sin_zero), 8);
  
  nSinsize = sizeof(struct sockaddr_in);
  if (connect(nSockwkr, (struct sockaddr *)&sinWkraddr, nSinsize) < 0)
  {
    printf("projb worker %d error: connect error! Exit!\n", nPid);
    return -1;
  }
  
  // first join the Triad ring, no need to use select
  
  // get initial information
  if (GetInitInfo(nSockwkr, Myname, FirstNodeName, &nMgrNonce, &FirstNodePort) < 0){
    errexit("client recv initial informatioin fail!\n");
  }
  
  // build logfilename
  snprintf(logfilename, sizeof(logfilename), "stage%d.%s.out", nStage, Myname);
  logfileinit(logfilename);
  
  printf("client: nonce %d, myname %s, firstNName %s, firstNPort %d\n", nMgrNonce, Myname, FirstNodeName, FirstNodePort);
  

  // create udp socket
  if ((udpSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    printf("projb worker %d error: create udp socket error! Exit!\n", nPid);
    return -1;
  }
  
  sinWkraddrudp.sin_family = AF_INET;
  sinWkraddrudp.sin_port = htons(0); // Random port
  sinWkraddrudp.sin_addr.s_addr = inet_addr("127.0.0.1");
  bzero(&(sinWkraddrudp.sin_zero), 8);
  
  // bind to udp socket
  if (bind(udpSock, (struct sockaddr *)&sinWkraddrudp, sizeof(sinWkraddrudp)) < 0){
    printf("projb worker %d error: udp socket bind error! Exit!\n", nPid);
    close(nSockwkr);
    return -1;
  }

  // get udp port number
  nSinsize = sizeof(struct sockaddr_in);
  if (getsockname(udpSock, (struct sockaddr *)&sinWkrcopy, &nSinsize) < 0){
    printf("projb worker %d error: udp socket getsockname error! Exit!\n", nPid);
    return -1;
  }
  MyUDPPort = ntohs(sinWkrcopy.sin_port);
  
  // Triad: calculate ID, join ring
  HashID = gethashid(nMgrNonce, Myname);
  HashIDfirst = gethashid(nMgrNonce, FirstNodeName);
  
  // debug message
  printf("client %d has name %s (%08x) with port %d\n", nPid, Myname, HashID, MyUDPPort);
  
  if (nStage < 4) {
    // stage < 4: join the ring
    if (JoinRing(udpSock) < 0) {
      printf("projb client %s error: join ring error! Exit!\n", Myname);
      close(nSockwkr);
      close(udpSock);
      return -1;
    }
  } else {
    // stage >= 4: we need a finger table
    InitFingerTableSelf();
    // join ring and initiate finger table
    if (JoinRingWithFingerTable(udpSock) < 0) {
      printf("projb client %s error: join ring error! Exit!\n", Myname);
      close(nSockwkr);
      close(udpSock);
      return -1;
    
    }
  }
  
  // send information back to manager
  nSum = nMgrNonce + nPid;
  snprintf(szSendbuf, sizeof(szSendbuf), "%d %d\n%d\n", nSum, nPid, MyUDPPort);

  nSendlen = strlen(szSendbuf);
  if (SendStreamData(nSockwkr, szSendbuf, nSendlen) < 0){
    printf("projb worker %d error: send back to manager error! Exit!\n", nPid);
    return -1;
  }
  
  // use select to multiplex communication
  tv.tv_sec = SELECT_TIMEOUT; 
  tv.tv_usec = 0;
  int sockMax;
  
  FD_ZERO(&allset);
  FD_SET(nSockwkr, &allset);
  sockMax = nSockwkr;
  
  FD_SET(udpSock, &allset);
  if (udpSock > sockMax) {  // reset the maximum socket
    sockMax = udpSock;
  }
  
  char  sstr[MAX_TEXT_SIZE] = {0};
  
  // main select loop
  while(1) {

    readset = allset;
    if (select(sockMax+1, &readset, NULL, NULL, &tv) == -1) {
      errexit("client select.\n");
    }
 
    // handle messages from other clients over UDP
    if (FD_ISSET(udpSock, &readset)) {
    //  printf("clinet %s (%08x %d) got something on UDP\n", Myname, HashID, MyUDPPort);
      if (HandleUdpMessage(udpSock) < 0){
        printf("projb client %s, HandleUdpMessage fails!\n", Myname);
        return -1;
      }
    }
   
    // handle messages from the manager over TCP (and graceful exiting)
    if (FD_ISSET(nSockwkr, &readset)){  // manager sock
      // mechanism for process exit: look at parent socket
      //printf("projb client %s, got sth from manager\n", Myname);
      memset(szRecvbuf, 0, sizeof(szRecvbuf));

      if ((nRecvbyte = RecvStreamLineForSelect(nSockwkr, szRecvbuf, sizeof(szRecvbuf))) <= 0)
      {
        // if parent exits, then we should exit too
        if (nRecvbyte == 0) {
          printf("projb client %s: parent socket close, exit!\n", Myname);
          break;
        } 
        else {
          printf("projb client %s: parent socket recv error, exit!\n", Myname);
          break;
        }
      }
      else {   // jobs assignment
        szRecvbuf[nRecvbyte-1] = '\0';
        if (strcmp(szRecvbuf, "exit!") == 0){
          //printf("projb client %s recv EXIT from manager!\n", Myname);

          // debug: log finger table
          /*
          if (nStage >= 4){
            LogFingerTable();
          }*/
          break;
        }
        else if(strcmp(szRecvbuf, "store") == 0) {
          // handle store command
          memset(sstr, 0, sizeof(sstr));
          if ((nRecvbyte = RecvStreamLineForSelect(nSockwkr, sstr, sizeof(sstr))) <= 0){
            printf("projb client %s error: recv store string from manager!\n", Myname);
            break;
          }
          sstr[nRecvbyte-1] = '\0';
          
          if (HandleStoreMsg(udpSock, sstr) < 0 ){
            printf("projb client %s error: handle store msg from manager!\n", Myname);
            break;
          }
          
        }
        else if(strcmp(szRecvbuf, "search") == 0) {
          // handle search command  
          memset(sstr, 0, sizeof(sstr));
          if ((nRecvbyte = RecvStreamLineForSelect(nSockwkr, sstr, sizeof(sstr))) <= 0){
            printf("projb client %s error: recv store string from manager!\n", Myname);
            break;
          }
          sstr[nRecvbyte-1] = '\0';
          
          if (HandleSearchMsg(udpSock, sstr) < 0 ){
            printf("projb client %s error: handle store msg from manager!\n", Myname);
            break;
          }
          
        }
        else if(strcmp(szRecvbuf, "end_client") == 0) {
          // handle end_client command
          memset(sstr, 0, sizeof(sstr));
          if ((nRecvbyte = RecvStreamLineForSelect(nSockwkr, sstr, sizeof(sstr))) <= 0){
            printf("projb client %s error: recv store string from manager!\n", Myname);
            break;
          }
          sstr[nRecvbyte-1] = '\0';

          // sanity check
          if (strcmp(sstr, Myname) != 0){
            printf("projb client %s error: target of end_client is not me! name: %s\n", Myname, sstr);
            break;
          }

          if (HandleEndClient(udpSock) < 0){
            printf("projb client %s error: handle end_client msg from manager!\n", Myname);
            break;
          }
        } // end handling of commands
        
        // tell manger ok
        snprintf(szSendbuf, sizeof(szSendbuf), "ok\n");
        nSendlen = strlen(szSendbuf);
        
        if (SendStreamData(nSockwkr, szSendbuf, nSendlen) < 0){
          printf("projb worker %d error: send ok back to manager error! Exit!\n", nPid);
          return -1;
        }

      } // end jobs assignment
    } // end manager socket handling
  } // end select loop
  
  close(nSockwkr);
  close(udpSock);
  
  return 0;
}

int GetInitInfo(int sock, char *selfname, char *firstnode, unsigned int *nonce, unsigned short *port){
  char szRecvbuf[256] = {0};
  int nRecvsize = 0;
  int nc;
  int p;
  
  // get nonce
  if ((nRecvsize = RecvStreamLineForSelect(sock, szRecvbuf, sizeof(szRecvbuf))) <= 0){
    return -1;
  }
  szRecvbuf[nRecvsize-1] = '\0'; // turn \n to \0
  nc = atoi(szRecvbuf);
  *nonce = (unsigned int)nc;
  
  // get selfname
  if ((nRecvsize = RecvStreamLineForSelect(sock, szRecvbuf, sizeof(szRecvbuf))) <= 0){
    return -1;
  }
  szRecvbuf[nRecvsize-1] = '\0'; // turn \n to \0
  strncpy(selfname, szRecvbuf, MAX_CLIENT_NAME_SIZE);
  
  // get first node udp port
  if ((nRecvsize = RecvStreamLineForSelect(sock, szRecvbuf, sizeof(szRecvbuf))) <= 0){
    return -1;
  }
  szRecvbuf[nRecvsize-1] = '\0'; // turn \n to \0
  if (strlen(szRecvbuf) == 1 && strcmp(szRecvbuf, "0") == 1){ // first node
    *port = 0;
  }else{
    p = atoi(szRecvbuf);
    *port = (unsigned short)p;
  }
  
  // get first node name
  if ((nRecvsize = RecvStreamLineForSelect(sock, szRecvbuf, sizeof(szRecvbuf))) <= 0){
    return -1;
  }
  szRecvbuf[nRecvsize-1] = '\0'; // turn \n to \0
  strncpy(firstnode, szRecvbuf, MAX_CLIENT_NAME_SIZE);
  
  return 0;
}

int JoinRing(int sock){
  TNode TempA, TempB;
  char  wbuf[128];
  int   msgtype;
  int flag = 0;
 
  // are we the first node of the ring?
  if (HashID == HashIDfirst) {
    succ.id = HashID;
    succ.port = MyUDPPort;
    pred.id = HashID;
    pred.port = MyUDPPort;
  }
  else {
    // we are not the first node
    if (HashID < HashIDfirst){ // counterclockwise
      msgtype = PREDQ;
      TempA.id = HashIDfirst;
      TempA.port = FirstNodePort;
      while(flag == 0){
        if (FindNeighbor(sock, msgtype, TempA, &TempB) < 0){ // errexit
          return -1;
        }
        // two situation to set flag
        // 1. B.hash >= A.hash
        // 2. HashID < A.hash && HashID > B.hash
        if (TempB.id >= TempA.id){
          flag = 1;
        }else if (HashID > TempB.id){
          flag = 1;
        }else if (HashID < TempB.id){ // A=B, B
          TempA.id = TempB.id;
          TempA.port = TempB.port;
        }
      }
      // tell A to change its predecessor, tell B to change its successor
      if (UpdateNeighbor(sock, &TempA, &TempB) < 0){
        return -1;
      }
      // A is the successor, B is predecessor
      succ.id = TempA.id;
      succ.port = TempA.port;
      pred.id = TempB.id;
      pred.port = TempB.port;
    } // end counterclockwise
    else { //clockwise
      msgtype = SUCCQ;
      TempA.id = HashIDfirst;
      TempA.port = FirstNodePort;
      
      while(flag == 0){
        if (FindNeighbor(sock, msgtype, TempA, &TempB) < 0){ // errexit
          return -1;
        }
        // two situation to set flag
        // 1. B.hash <= A.hash
        // 2.  HashID < B.hash
        if (TempB.id <= TempA.id){
          flag = 1;
        }else if (HashID < TempB.id){
          flag = 1;
        }else if (HashID > TempB.id){ // set A to B
          TempA.id = TempB.id;
          TempA.port = TempB.port;
        }
      }
      // tell B to change its predecessor, tell A to change its successor
      if (UpdateNeighbor(sock, &TempB, &TempA) < 0){
        return -1;
      }
      
      succ.id = TempB.id;
      succ.port = TempB.port;
      pred.id = TempA.id;
      pred.port = TempA.port;
    } // end clockwise
  } // end joining of the ring
  
  // join finishes, write to log
  snprintf(wbuf, sizeof(wbuf), "client %s created with hash 0x%08x\n", Myname, HashID);
  logfilewriteline(logfilename, wbuf, strlen(wbuf));
  
  return 0;
}

int JoinRingWithFingerTable(int sock){
  char  wbuf[128];
  
  if (HashID != HashIDfirst){ // not first node
    if (InitFingerTable(sock) < 0){
      printf("projb client %s: InitFingerTable fails!\n", Myname);
      return -1;
    }

    /**************** for debug*/
    //LogFingerTable();
    
    if (UpdateOthers(sock) < 0){
      printf("projb client %s: UpdateOthers fails!\n", Myname);
      return -1;
    }
  }

  // join finishes, write log
  snprintf(wbuf, sizeof(wbuf), "client %s created with hash 0x%08x\n", Myname, HashID);
  logfilewriteline(logfilename, wbuf, strlen(wbuf));
  
  return 0;
}

int InitFingerTable(int sock){
  TNode mysucc;
  TNode mypred;
  int i;

  // get my successor
  if (FindSuccWithFT(sock, HashID, &mysucc) < 0){
    printf("projb client %s: InitFingerTable->FindSuccWithFT find successor fails!\n", Myname);
    return -1;
  }
  // set my successor
  succ.id = mysucc.id;
  succ.port = mysucc.port;

  MyFT[1].node.id = mysucc.id;
  MyFT[1].node.port = mysucc.port;

  // get my predecessor
  if (FindNeighbor(sock, PREDQ, mysucc, &mypred) < 0){
    printf("projb client %s: InitFingerTable->FindNeighbor find predecessor fails!\n", Myname);
    return -1;
  }
  // set my predecessor
  pred.id = mypred.id;
  pred.port = mypred.port;
  MyFT[0].node.id = mypred.id;
  MyFT[0].node.port = mypred.port;

  // update my finger table
  // note that now I am not a node in the ring, so this round of initiate may not correct,
  // will update my finger table later.
  for (i=1; i<(FTLEN-1); i++){
    if (InRangeA(HashID, MyFT[i+1].start, MyFT[i].node.id)){
      MyFT[i+1].node.id = MyFT[i].node.id;
      MyFT[i+1].node.port = MyFT[i].node.port;
    }else{
      //find succ
      TNode tn;
      if (FindSuccWithFT(sock, MyFT[i+1].start, &tn) <  0){
        printf("projb client %s: InitFingerTable->FindSuccWithFT(in loop)  fails! \n", Myname);
        return -1;
      }
      MyFT[i+1].node.id = tn.id;
      MyFT[i+1].node.port = tn.port;
    }
  }

  //update their neighbour
  if (UpdateNeighbor(sock, &mysucc, &mypred) < 0){
    printf("projb client %s: InitFingerTable->UpdateNeighbor fails! \n", Myname);
    return -1;
  }

  // now update my finger table
  // just look at those finger table entries whose node is my successor
  UpdateMyFingerTableInit();

  return 0;
}

void UpdateMyFingerTableInit(){
  int i;
  for (i=1; i<FTLEN; i++){
    if (MyFT[i].node.id == succ.id){
      if (NotInRange(MyFT[i].start, HashID, succ.id)){
        MyFT[i].node.id = HashID;
        MyFT[i].node.port = MyUDPPort;
      }
    }
  }
}

int UpdateOthers(int sock){
  int i;
  unsigned int nTemp;
  unsigned int exp;
  TNode tempsu;
  TNode temppr;
  TNode myself;

  for (i=1; i<FTLEN; i++){
    // circle minus
    exp = (unsigned int)(1<<(i-1));

    nTemp = RingMinus(HashID, exp);
    /*if (n >= exp){
      nTemp = n - exp;
    }
    else{
      nTemp = HASHMAX - (exp - n -1);
    }*/

    // find predecessor => find successor and find successor's predecessor
    if(FindSuccWithFT(sock, nTemp, &tempsu) < 0){
      printf("projb client %s: UpdateOthers->FindSuccWithFT fails!\n", Myname);
      return -1;
    }

    /**************** for debug **********
    char wbuf[128];
    snprintf(wbuf, sizeof(wbuf), "UpdateOthers find succ of %08x, succ is %08x \n", nTemp, tempsu.id);
    logfilewriteline(logfilename, wbuf, strlen(wbuf));
    **********/

    if (FindNeighbor(sock, PREDQ, tempsu, &temppr) < 0){
      printf("projb client %s: UpdateOthers->FindNeighbor find predecessor fails!\n", Myname);
      return -1;
    }

    /**************** for debug **********
    snprintf(wbuf, sizeof(wbuf), "UpdateOthers find pred of %08x, pred is %08x \n", nTemp, temppr.id);
    logfilewriteline(logfilename, wbuf, strlen(wbuf));
    *********/

    myself.id = HashID;
    myself.port = MyUDPPort;

    // if it's myself
    if(temppr.id == HashID){
      /*** never update myself in update others ****
      **********************************************
      if (UpdateMyFingerTable(sock, myself, i) < 0){
        printf("projb client %s: UpdateOthers->UpdateMyFingerTable fails!\n", Myname);
        return -1;
      }************/
      //
      continue;
    }

    
    if (UpdateFingerTable(sock, temppr, myself, i) < 0){
      printf("projb client %s: UpdateOthers->UpdateFingerTable 0x%08x %d fails!\n", Myname, temppr.id, i);
      return -1;
    }
    // if other nodes, do it iteratively
    /*
    tempFstart = RingPlus(temppr.id, exp);
    while(!NotInRange(tempFstart, pred.id, HashID)){
      if (UpdateFingerTable(sock, temppr, i) < 0){
        printf("projb client %s: UpdateOthers->UpdateFingerTable 0x%08x %d fails!\n", Myname, temppr.id, i);
        return -1;
      }

      // get temppr's predecessor
      if (FindNeighbor(sock, PREDQ, temppr, &temppr2) < 0){
        printf("projb client %s: UpdateOthers->FindNeighbor in loop find predecessor fails!\n", Myname);
        return -1;
      }

      temppr.id = temppr2.id;
      temppr.port = temppr2.port;

      tempFstart = RingPlus(temppr.id, exp);
    }*/
  }
  return 0;
}

void InitFingerTableSelf(){
  unsigned int diff;
  int i;
  
  unsigned int fstr;
  unsigned int fend;
  unsigned int itv;
  
  // init predecessor
  MyFT[0].start = HashID;
  MyFT[0].end = HashID;
  MyFT[0].node.id = HashID;
  MyFT[0].node.port = MyUDPPort;

  succ.id = HashID;
  succ.port = MyUDPPort;
  pred.id = HashID;
  pred.port = MyUDPPort;
  
  // start from 1, init FT
  for (i=1; i<FTLEN; i++){
    itv = (1 << (i-1));
    diff = HASHMAX - HashID;
    
    // int overflow free
    if (diff < itv){
      fstr = itv - diff - 1;
    }else{
      fstr = HashID + itv;
    }
    
    if (i == FTLEN-1){
      fend = HashID;
    }else{
      itv = (1<<i);
      diff = HASHMAX - HashID;
      if (diff < itv){
        fend = itv - diff - 1;
      }else{
        fend = HashID + itv;
      }
    }
    MyFT[i].start = fstr;
    MyFT[i].end = fend;
    MyFT[i].node.id = HashID;
    MyFT[i].node.port = MyUDPPort;
  }
}

int FindNeighbor(int sock, int msgtype, TNode na, TNode *pnb){
  NGQM  qrymsg;
  NGRM  *pngr;
  struct  sockaddr_in naaddr;
  char  sendbuf[128];
  char  recvbuf[128];
  //char  writebuf[256];
  int nSendbytes;
  int nRecvbytes;

  // prevent to send self query message
  if (na.id == HashID){
    if (msgtype == SUCCQ){
      pnb->id = succ.id;
      pnb->port = succ.port;
    }
    else if(msgtype == PREDQ){
      pnb->id = pred.id;
      pnb->port = pred.port;
    }

    return 0;
  }
  
  qrymsg.msgid = htonl(msgtype);
  qrymsg.ni = htonl(na.id);
  memcpy(sendbuf, &qrymsg, sizeof(NGQM));
  
  naaddr.sin_family = AF_INET;
  naaddr.sin_port = htons(na.port);
  naaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  
  // to be simple, just assume sendto always send all data required.
  if ((nSendbytes = sendto(sock, sendbuf, sizeof(NGQM), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != sizeof(NGQM)){
    printf("projb error: find_neighbor sendto ret %d, should send %lu\n", nSendbytes, sizeof(NGQM));
    return -1;
  }
  
  // wirte log
  LogTyiadMsg(msgtype, SENTFLAG, sendbuf);
  
  //printf("clinet %s (%08x %d) sent successor-q (%08x %d)\n", Myname, HashID, MyUDPPort, na.id, na.port);
  
  // recvfrom, assume it recv all we need
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
  if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(NGRM), 0, NULL, NULL)) != sizeof(NGRM)){
    printf("projb error: find_neighbor recvfrom ret %d, should recv %lu\n", nRecvbytes, sizeof(NGRM));
    return -1;
  }
  
  // wirte log
  LogTyiadMsg((msgtype+1), RECVFLAG, recvbuf);
  
  //parse data
  pngr = (pngrm)recvbuf;
  // sanity check
  if ((msgtype+1) != ntohl(pngr->msgid) || na.id != ntohl(pngr->ni)){
    printf("projb error: find_neighbor recv ngr msg %d %d, ni %08x %08x\n", msgtype, ntohl(pngr->msgid), na.id, ntohl(pngr->ni));
    return -1;
  }
  
  pnb->id = ntohl(pngr->si);
  pnb->port = ntohl(pngr->sp);
  
  return 0;
}

int FindSuccWithFT(int sock, unsigned int id, TNode *retnode){
  TNode tempA;
  TNode tempB;
  // start from first node
  tempA.id = HashIDfirst;
  tempA.port = FirstNodePort;
  
  // find first node successor.
  if (HashID == HashIDfirst){ // I am the first node
    tempB.id = succ.id;
    tempB.port = succ.port;
  }else{
    if (FindNeighbor(sock, SUCCQ, tempA, &tempB) < 0){
      printf("projb client %s: FindSuccWithFT->FindNeighbor fail, tempA 0x%08x \n", Myname, tempA.id);
      return -1;
    }
  }
  
  while(NotInRange(id, tempA.id, tempB.id)){
    // find closest 
    if (FindClosest(sock, CLSTQ, id, tempA, &tempB)<0){
      printf("projb client %s: FindSuccWithFT->FindClosest fail, tempA 0x%08x id 0x%08x \n", Myname, tempA.id, id);
      return -1;
    }

    // handle weired situation here
    // if A.id == B.id, there are two situation, 
    // 1. id belongs to A, 
    // 2. id belogns to A.successor
    // need some judgment here
    if (tempA.id == tempB.id){
      //find A's successor
      if (FindNeighbor(sock, SUCCQ, tempA, &tempB) < 0){
        printf("projb client %s: FindSuccWithFT->FindNeighbor fail, tempA 0x%08x \n", Myname, tempA.id);
        return -1;
      }

      if (tempA.id == tempB.id){  // first node, and we are the second node.
        retnode->id = tempA.id;
        retnode->port = tempA.port;
        return 0;
      }

      if (NotInRange(id, tempA.id, tempB.id)){ // A is successor
        retnode->id = tempA.id;
        retnode->port = tempA.port;
        return 0;
      }else{
        retnode->id = tempB.id;
        retnode->port  = tempB.port;
        return 0;
      }
    }

    tempA.id = tempB.id;
    tempA.port = tempB.port;

    //find A's successor
    if (FindNeighbor(sock, SUCCQ, tempA, &tempB) < 0){
      printf("projb client %s: FindSuccWithFT->FindNeighbor fail, tempA 0x%08x \n", Myname, tempA.id);
      return -1;
    }
  }

  retnode->id = tempB.id;
  retnode->port = tempB.port;
  
  return 0;
}


int UpdateNeighbor(int sock, TNode *chgpreNode, TNode *chgsucNode){
  struct  sockaddr_in naaddr;
  char  sendbuf[128];
  char  recvbuf[128];
  int nSendbytes, nRecvbytes;
  UPQM  updmsg;
  
  // first change someone's predecessor
  naaddr.sin_family = AF_INET;
  naaddr.sin_port = htons(chgpreNode->port);
  naaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  
  
  updmsg.msgid = htonl(UPDTQ);
  updmsg.ni = htonl(chgpreNode->id);
  updmsg.si = htonl(HashID);
  updmsg.sp = htonl((int)MyUDPPort);
  updmsg.i = htonl(0); // change predecessor
  memcpy(sendbuf, &updmsg, sizeof(UPQM));
  
  if ((nSendbytes = sendto(sock, sendbuf, sizeof(UPQM), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != sizeof(UPQM)){
    printf("projb error: update_neighbor sendto ret %d, should send %lu\n", nSendbytes, sizeof(UPQM));
    return -1;
  }
  
  // log
  LogTyiadMsg(UPDTQ, SENTFLAG, sendbuf);

  // only receive reply after stage 4
  if (nStage >= 2){
    // receive reply
    /**********************************************************************
    *** for stage >= 6, needs to judge if the hello messages comes here ******
    **********************************************************************/
    if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(UPRM), 0, NULL, NULL)) != sizeof(UPRM)){
      printf("projb error: update_neighbor recvfrom ret %d, should recv %lu\n", nRecvbytes, sizeof(UPRM));
      return -1;
    }
    LogTyiadMsg(UPDTR, RECVFLAG, recvbuf);
  }
  
  // then, update someone's successor
  naaddr.sin_port = htons(chgsucNode->port);
  
  updmsg.ni = htonl(chgsucNode->id);
  updmsg.i = htonl(1); // update successor
  memset(sendbuf, 0, sizeof(sendbuf));
  memcpy(sendbuf, &updmsg, sizeof(UPQM));
  
  if ((nSendbytes = sendto(sock, sendbuf, sizeof(UPQM), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != sizeof(UPQM)){
    printf("projb error: update_neighbor sendto ret %d, should send %lu\n", nSendbytes, sizeof(UPQM));
    return -1;
  }
  
  // log
  LogTyiadMsg(UPDTQ, SENTFLAG, sendbuf);

  if (nStage >= 2){
    // receive reply
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
    if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(UPRM), 0, NULL, NULL)) != sizeof(UPRM)){
      printf("projb error: update_neighbor recvfrom ret %d, should recv %lu\n", nRecvbytes, sizeof(UPRM));
      return -1;
    }
    LogTyiadMsg(UPDTR, RECVFLAG, recvbuf);
  }
  
  return 0;
}

int UpdateFingerTable(int sock, TNode tn, TNode sn, int idx){
  struct  sockaddr_in naaddr;
  char  sendbuf[128];
  char  recvbuf[128];
  int nSendbytes, nRecvbytes;
  UPQM  updmsg;
  puprm ptr;
  
  naaddr.sin_family = AF_INET;
  naaddr.sin_port = htons(tn.port);
  naaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  
  
  updmsg.msgid = htonl(UPDTQ);
  updmsg.ni = htonl(tn.id);
  updmsg.si = htonl(sn.id);
  updmsg.sp = htonl((int)(sn.port));
  updmsg.i = htonl(idx);
  memcpy(sendbuf, &updmsg, sizeof(UPQM));


  if ((nSendbytes = sendto(sock, sendbuf, sizeof(UPQM), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != sizeof(UPQM)){
    printf("projb error: UpdateFingerTable sendto ret %d, should send %lu\n", nSendbytes, sizeof(UPQM));
    return -1;
  }

  // log
  LogTyiadMsg(UPDTQ, SENTFLAG, sendbuf);

  // receive reply
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
  if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(UPRM), 0, NULL, NULL)) != sizeof(UPRM)){
    printf("projb error: UpdateFingerTable recvfrom ret %d, should recv %lu\n", nRecvbytes, sizeof(UPRM));
    return -1;
  }

  // sanity check 
  ptr = (puprm)recvbuf;
  if (ntohl(ptr->msgid) != UPDTR){
    printf("projb client %s: UpdateFingerTable recvfrom unexpect message %d!\n", Myname, ntohl(ptr->msgid));
    return -1;
  }
  if ((unsigned int)ntohl(ptr->ni) != tn.id || 
    (unsigned int)ntohl(ptr->si) != sn.id ||
    ntohl(ptr->sp) != (int)sn.port || ntohl(ptr->i) != idx){
    printf("projb client %s: UpdateFingerTable recvfrom wrong message: (0x%08x 0x%08x %d %d)\n", 
      Myname, ntohl(ptr->ni), ntohl(ptr->si),  ntohl(ptr->sp),  ntohl(ptr->i));
    return -1;
  }
  // wirte log
  LogTyiadMsg(UPDTR, RECVFLAG, recvbuf);

  return 0;
}

int UpdateMyFingerTable(int sock, TNode s, int idx){

  if (InRangeA(HashID, s.id, MyFT[idx].node.id)){
    MyFT[idx].node.id = s.id;
    MyFT[idx].node.port = s.port;
    
    if (pred.id != s.id){
      // update pred finger
      if (UpdateFingerTable(sock, pred, s, idx) < 0){
        printf("projb client %s: UpdateMyFingerTable update pred fails!\n", Myname);
        return -1;
      }
    }
  }

  return 0;
}

int HandleUdpMessage(int sock){
  struct sockaddr_in  cliaddr;
  //int sa_len = sizeof(cliaddr);
  socklen_t sa_len = sizeof(cliaddr);
  char  recvbuf[256];
  char  sendbuf[256];
  int sendlen;
  int recvlen;
  int msgtype;
  int *tmp;
  pupqm ptr;
  int ret;
  
  
  // first recv 4 bytes to see which type of message
  if ((recvlen = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&cliaddr, &sa_len)) < 0){
    printf("projb error: HandleUdpMessage recvfrom faile!\n");
    return -1;
  }
  //printf("clinet %s (%08x %d) recvd msg type\n", Myname, HashID, MyUDPPort);
  tmp = (int *)recvbuf;
  
  msgtype = ntohl(*tmp);
  
  switch(msgtype){
  case SUCCQ:
//    if ((recvlen = recvfrom(sock, recvbuf+sizeof(int), sizeof(int), 0, (struct sockaddr *)&cliaddr, &sa_len)) != sizeof(int)){
//      printf("projb error: HandleUdpMessage recvfrom ret %d, shoulde recv %d\n", recvlen, sizeof(int));
//      return -1;
//    }
    //sanity check
  {
    if(recvlen != sizeof(NGQM)){  
      printf("projb error: clinet %s (%08x %d) recvd successor-q, but length wrong %d, should be %lu\n", Myname, HashID, MyUDPPort, recvlen, sizeof(NGQM));
      return -1;
    }
    // printf("clinet %s (%08x %d) recvd successor-q, lenght %d\n", Myname, HashID, MyUDPPort, recvlen);
    // log
    LogTyiadMsg(SUCCQ, RECVFLAG, recvbuf);
    
    NGRM sucrlp;
    sucrlp.msgid = htonl(SUCCR);
    sucrlp.ni = htonl(HashID);
    sucrlp.si = htonl(succ.id);
    sucrlp.sp = htonl(succ.port);
    memcpy(sendbuf, &sucrlp, sizeof(sucrlp));
    if ((sendlen = sendto(sock, sendbuf, sizeof(sucrlp), 0, (struct sockaddr *)&cliaddr, sa_len)) != sizeof(sucrlp)){
      printf("projb error: HandleUdpMessage sendto ret %d, shoulde send %lu\n", sendlen, sizeof(sucrlp));
      return -1;
    }
    LogTyiadMsg(SUCCR, SENTFLAG, sendbuf);
  }
    break;
  case PREDQ:
                //printf("clinet %s (%08x %d) recvd predecessor-q, length %d\n", Myname, HashID, MyUDPPort, recvlen);
  //  if ((recvlen = recvfrom(sock, recvbuf+sizeof(int), sizeof(int), 0, (struct sockaddr *)&cliaddr, &sa_len)) != sizeof(int)){
  //    printf("projb error: HandleUdpMessage recvfrom ret %d, shoulde recv %d\n", recvlen, sizeof(int));
  //    return -1;
  //  }
    // log
  {
    LogTyiadMsg(PREDQ, RECVFLAG, recvbuf);
    
    NGRM prcrlp;
    prcrlp.msgid = htonl(PREDR);
    prcrlp.ni = htonl(HashID);
    prcrlp.si = htonl(pred.id);
    prcrlp.sp = htonl(pred.port);
    memcpy(sendbuf, &prcrlp, sizeof(prcrlp));
    if ((sendlen = sendto(sock, sendbuf, sizeof(prcrlp), 0, (struct sockaddr *)&cliaddr, sa_len)) != sizeof(prcrlp)){
      printf("projb error: HandleUdpMessage sendto ret %d, should send %lu\n", sendlen, sizeof(prcrlp));
      return -1;
    }
    LogTyiadMsg(PREDR, SENTFLAG, sendbuf);
  }
    break;
  case UPDTQ:
    //printf("clinet %s (%08x %d) recvd update-q, length %d\n", Myname, HashID, MyUDPPort, recvlen);
  //  if ((recvlen = recvfrom(sock, recvbuf+sizeof(int), (sizeof(UPQM)-sizeof(int)), 0, (struct sockaddr *)&cliaddr, &sa_len)) != sizeof(int)){
  //    printf("projb error: HandleUdpMessage recvfrom ret %d, shoulde recv %d\n", recvlen, (sizeof(UPQM)-sizeof(int)));
  //    return -1;
  //  }
    // log
  {
    LogTyiadMsg(UPDTQ, RECVFLAG, recvbuf);
    TNode t;
    int idx;
    
    ptr = (pupqm)recvbuf;
    idx = ntohl(ptr->i);

    if (idx == 0){ //update predecessor
      pred.id = ntohl(ptr->si);
      pred.port = ntohl(ptr->sp);
      if (nStage >= 2){
        MyFT[0].node.id = pred.id;
        MyFT[0].node.port = pred.port;

        // send reply 
        UPRM uprmsg;
        uprmsg.msgid = htonl(UPDTR);
        uprmsg.ni = htonl(HashID);
        uprmsg.r = htonl(1);
        uprmsg.si = ptr->si;
        uprmsg.sp = ptr->sp;
        uprmsg.i = ptr->i;
        memcpy(sendbuf, &uprmsg, sizeof(UPRM));
        if ((sendlen = sendto(sock, sendbuf, sizeof(UPRM), 0, (struct sockaddr *)&cliaddr, sa_len)) != sizeof(UPRM)){
          printf("projb error: client %s HandleUdpMessage update-r send ret %d, shoulde send %lu\n", Myname, sendlen, sizeof(UPRM));
          return -1;
        }
        LogTyiadMsg(UPDTR, SENTFLAG, sendbuf);
      }
    }else if(idx == 1){ // update successor
      succ.id = ntohl(ptr->si);
      succ.port = ntohl(ptr->sp);
      if (nStage >= 2){
        MyFT[1].node.id = succ.id;
        MyFT[1].node.port = succ.port;

        // send reply 
        UPRM uprmsg;
        uprmsg.msgid = htonl(UPDTR);
        uprmsg.ni = htonl(HashID);
        uprmsg.r = htonl(1);
        uprmsg.si = ptr->si;
        uprmsg.sp = ptr->sp;
        uprmsg.i = ptr->i;
        memcpy(sendbuf, &uprmsg, sizeof(UPRM));
        if ((sendlen = sendto(sock, sendbuf, sizeof(UPRM), 0, (struct sockaddr *)&cliaddr, sa_len)) != sizeof(UPRM)){
          printf("projb error: client %s HandleUdpMessage update-r send ret %d, should send %lu\n", Myname, sendlen, sizeof(UPRM));
          return -1;
        }
        LogTyiadMsg(UPDTR, SENTFLAG, sendbuf);
      }
    }else{
      if (nStage < 4){
        printf("projb exception: unknown update-q message %d in stage2.\n", ntohl(ptr->i));
      }else{
        t.id = ntohl(ptr->si);
        t.port = ntohl(ptr->sp);
        if (UpdateMyFingerTable(sock, t, idx)<0){
          printf("projb error: client %s HandleUdpMessage update message UpdateMyFingerTable fails!\n", Myname);
          return -1;
        }

        // send reply 
        UPRM uprmsg;
        uprmsg.msgid = htonl(UPDTR);
        uprmsg.ni = htonl(HashID);
        uprmsg.r = htonl(1);
        uprmsg.si = htonl(t.id);
        uprmsg.sp = htonl(t.port);
        uprmsg.i = htonl(idx);
        memcpy(sendbuf, &uprmsg, sizeof(UPRM));
        if ((sendlen = sendto(sock, sendbuf, sizeof(UPRM), 0, (struct sockaddr *)&cliaddr, sa_len)) != sizeof(UPRM)){
          printf("projb error: client %s HandleUdpMessage update-r send ret %d, should send %lu\n", Myname, sendlen, sizeof(UPRM));
          return -1;
        }
        LogTyiadMsg(UPDTR, SENTFLAG, sendbuf);
      }
    }
  }
    break;
  case CLSTQ:
  {
    LogTyiadMsg(CLSTQ, RECVFLAG, recvbuf);
    pclqm clqptr;
    CLRM  clrmsg;
    clqptr = (pclqm)recvbuf;
    TNode tempN;
    unsigned int ndi = ntohl(clqptr->di);
    if (pred.id > HashID){  // I'm the first node
      if (ndi <= HashID || ndi > pred.id){  // I should have it
        ret = SearchClientStore(ndi, NULL);
        clrmsg.msgid = htonl(CLSTR);
        clrmsg.ni = htonl(HashID);
        clrmsg.di = htonl(ndi);
        clrmsg.ri = htonl(HashID);
        clrmsg.rp = htonl(MyUDPPort);
        if (ret == 1){
          clrmsg.nhas = htonl(ret);
        }else{
          clrmsg.nhas = htonl(0);
        }
      }else{
        if (nStage < 4){
          clrmsg.msgid = htonl(CLSTR);
          clrmsg.ni = htonl(HashID);
          clrmsg.di = htonl(ndi);
          clrmsg.ri = htonl(succ.id);
          clrmsg.rp = htonl(succ.port);
          clrmsg.nhas = htonl(0);
        }else{
          ClosestPrecedingFinger(ndi, &tempN);
          clrmsg.msgid = htonl(CLSTR);
          clrmsg.ni = htonl(HashID);
          clrmsg.di = htonl(ndi);
          clrmsg.ri = htonl(tempN.id);
          clrmsg.rp = htonl(tempN.port);
          clrmsg.nhas = htonl(0);
        }
      }
    }
    else{
      if (ndi <= HashID){
        if(ndi > pred.id || ndi == HashID){ // I should have it
          ret = SearchClientStore(ndi, NULL);
          clrmsg.msgid = htonl(CLSTR);
          clrmsg.ni = htonl(HashID);
          clrmsg.di = htonl(ndi);
          clrmsg.ri = htonl(HashID);
          clrmsg.rp = htonl(MyUDPPort);
          if (ret == 1){
            clrmsg.nhas = htonl(ret);
          }else{
            clrmsg.nhas = htonl(0);
          }
        }else{    
          if (nStage < 4){
            // predecessor is the estimation
            clrmsg.msgid = htonl(CLSTR);
            clrmsg.ni = htonl(HashID);
            clrmsg.di = htonl(ndi);
            clrmsg.ri = htonl(pred.id);
            clrmsg.rp = htonl(pred.port);
            clrmsg.nhas = htonl(0);
          }else{
            ClosestPrecedingFinger(ndi, &tempN);
            clrmsg.msgid = htonl(CLSTR);
            clrmsg.ni = htonl(HashID);
            clrmsg.di = htonl(ndi);
            clrmsg.ri = htonl(tempN.id);
            clrmsg.rp = htonl(tempN.port);
            clrmsg.nhas = htonl(0);
          }
        }
      }else{   
        if (nStage < 4){
          // successor is the estimation
          clrmsg.msgid = htonl(CLSTR);
          clrmsg.ni = htonl(HashID);
          clrmsg.di = htonl(ndi);
          clrmsg.ri = htonl(succ.id);
          clrmsg.rp = htonl(succ.port);
          clrmsg.nhas = htonl(0);
        }else{
          ClosestPrecedingFinger(ndi, &tempN);
          clrmsg.msgid = htonl(CLSTR);
          clrmsg.ni = htonl(HashID);
          clrmsg.di = htonl(ndi);
          clrmsg.ri = htonl(tempN.id);
          clrmsg.rp = htonl(tempN.port);
          clrmsg.nhas = htonl(0);
        }
      }
    }
    //send reply message
    memcpy(sendbuf, &clrmsg, sizeof(CLRM));
    if ((sendlen = sendto(sock, sendbuf, sizeof(CLRM), 0, (struct sockaddr *)&cliaddr, sa_len)) != sizeof(CLRM)){
      printf("projb error: HandleUdpMessage CLSTR sendto ret %d, shoulde send %lu\n", sendlen, sizeof(CLRM));
      return -1;
    }
    LogTyiadMsg(CLSTR, SENTFLAG, sendbuf);
  }
    break;
  case STORQ:
  {
    LogTyiadMsg(STORQ, RECVFLAG, recvbuf);
    
    unsigned int  id;
    int len;
    char  s[MAX_TEXT_SIZE];
    STRM  strlymsg;
    pstqm stqptr;
    stqptr = (pstqm)recvbuf;
    
    len = ntohl(stqptr->sl);
    recvbuf[sizeof(STQM)+len] = '\0';
    id = gethashid(nMgrNonce, (recvbuf+sizeof(STQM)));
    
    memcpy(s, recvbuf+sizeof(STQM), len);
    s[len] = '\0';
    
    if (pred.id > HashID){ // I'm the smallest
      if (id > pred.id || id <= HashID){   // I will store it
        if(SearchClientStore(id, s) == 1){  // already stored
          strlymsg.r = htonl(2);
        }else{
          AddClientStore(id, s);
          strlymsg.r = htonl(1);
        }
      }else{   // not mine
        strlymsg.r = 0;
      }
    }else{ //  I'm not the smallest
      if ( id > pred.id && id <= HashID){   // store it
        if(SearchClientStore(id, s) == 1){  // already stored
          strlymsg.r = htonl(2);
        }else{
          AddClientStore(id, s);
          strlymsg.r = htonl(1);
        }
      }else{  // not mine
        strlymsg.r = 0;
      }
    }
    
    strlymsg.msgid = htonl(STORR);
    strlymsg.ni = htonl(HashID);
    strlymsg.sl = htonl(strlen(s));
    memcpy(sendbuf, &strlymsg, sizeof(STRM));
    memcpy(sendbuf+sizeof(STRM), s, strlen(s));
    if ((sendlen = sendto(sock, sendbuf, (sizeof(STRM)+strlen(s)), 0, (struct sockaddr *)&cliaddr, sa_len)) != (sizeof(STRM)+strlen(s))){
      printf("projb error: HandleUdpMessage STORR sendto ret %d, should send %lu\n", sendlen, (sizeof(STRM)+strlen(s)));
      return -1;
    }
    LogTyiadMsg(STORR, SENTFLAG, sendbuf);
    
    // log store success
  }
    break;
  case LEAVQ:
    {
      LogTyiadMsg(LEAVQ, RECVFLAG, recvbuf);
      pleqm pleave;
      unsigned int leaveid;

      pleave = (pleqm)recvbuf;
      leaveid = (unsigned int)ntohl(pleave->di);

      if (leaveid == pred.id){   // I am the successor of leaving node
        // fetch data
        NXQM  nxqmsg;
        LERM  lermsg;
        pnxrm pnxrptr;
        unsigned int nxqid;
        unsigned int hid;
        int   length, i;
        char textbuf[MAX_STORE_TEXT_SIZE];
        nxqid = leaveid;
        while(1){
          // send NXTDQ messgae
          nxqmsg.msgid = htonl(NXTDQ);
          nxqmsg.di = htonl(leaveid);
          nxqmsg.id = htonl(nxqid);
          memcpy(sendbuf, &nxqmsg, sizeof(NXQM));
          if ((sendlen = sendto(sock, sendbuf, sizeof(NXQM), 0, (struct sockaddr *)&cliaddr, sa_len)) != sizeof(NXQM)){
            printf("projb client %s error: HandleUdpMessage NXTDQ sendto ret %d, should send %lu\n", Myname, sendlen, sizeof(NXQM));
            return -1;
          }
          LogTyiadMsg(NXTDQ, SENTFLAG, sendbuf);

          // recv NXTDR
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
          if ((recvlen = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, NULL, NULL)) < 0){
            printf("projb %s error: HandleUdpMessage NXTDR recvfrom fail\n", Myname);
            return -1;
          }
          LogTyiadMsg(NXTDR, RECVFLAG, recvbuf);
          pnxrptr = (pnxrm)recvbuf;
          length = ntohl(pnxrptr->sl);
          if (length == 0){  // no data to fethch
            break;
          }else{
            //copy string out and store it
            memset(textbuf, 0, MAX_STORE_TEXT_SIZE);
            memcpy(textbuf, recvbuf+sizeof(NXRM), length);
            textbuf[sizeof(NXRM)+length] = '\0';

            hid = gethashid(nMgrNonce, textbuf);
            AddClientStore(hid, textbuf);

            nxqid = ntohl(pnxrptr->rid);
          }
        }
        // update my finger table
        for (i=1; i<FTLEN; i++){
          if (MyFT[i].node.id == leaveid){
            MyFT[i].node.id = HashID;
            MyFT[i].node.port = MyUDPPort;
          }
        }
        // send leav-r
        lermsg.msgid = htonl(LEAVR);
        lermsg.ni = htonl(HashID);
        memcpy(sendbuf, &lermsg, sizeof(LERM));
        if ((sendlen = sendto(sock, sendbuf, sizeof(LERM), 0, (struct sockaddr *)&cliaddr, sa_len)) != sizeof(LERM)){
          printf("projb client %s error: HandleUdpMessage NXTDQ sendto ret %d, should send %lu\n", Myname, sendlen, sizeof(LERM));
          return -1;
        }
        LogTyiadMsg(LEAVR, SENTFLAG, sendbuf);
      }else{
        // get its successor
        int i;
        NGQM  qrymsg;
        NGRM  *pngr;
        LERM  lermsg;
        TNode lesucc;
        qrymsg.msgid = htonl(SUCCQ);
        qrymsg.ni = htonl(leaveid);
        memcpy(sendbuf, &qrymsg, sizeof(NGQM));
  
        if ((sendlen = sendto(sock, sendbuf, sizeof(NGQM), 0, (struct sockaddr *)&cliaddr, sa_len)) != sizeof(NGQM)){
          printf("projb client %s error: HandleUdpMessage succ-q sendto ret %d, should send %lu\n", Myname, sendlen, sizeof(NGQM));
          return -1;
        }
        LogTyiadMsg(SUCCQ, SENTFLAG, sendbuf);
  
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
        if ((recvlen = recvfrom(sock, recvbuf, sizeof(NGRM), 0, NULL, NULL)) != sizeof(NGRM)){
          printf("projb error: HandleUdpMessage succ-r recvfrom ret %d, should recv %lu\n", recvlen, sizeof(NGRM));
          return -1;
        }
  
        LogTyiadMsg(SUCCR, RECVFLAG, recvbuf);
  
        pngr = (pngrm)recvbuf;
        lesucc.id = ntohl(pngr->si);
        lesucc.port = ntohl(pngr->sp);

        //update my finger table
        for (i=1; i<FTLEN; i++){
          if (MyFT[i].node.id == leaveid){
            MyFT[i].node.id = lesucc.id;
            MyFT[i].node.port = lesucc.port;
          }
        }
        // send leav-r

        lermsg.msgid = htonl(LEAVR);
        lermsg.ni = htonl(HashID);
        memcpy(sendbuf, &lermsg, sizeof(LERM));
        if ((sendlen = sendto(sock, sendbuf, sizeof(LERM), 0, (struct sockaddr *)&cliaddr, sa_len)) != sizeof(LERM)){
          printf("projb client %s error: HandleUdpMessage NXTDQ sendto ret %d, should send %lu\n", Myname, sendlen, sizeof(LERM));
          return -1;
        }
        LogTyiadMsg(LEAVR, SENTFLAG, sendbuf);
      }
    }
    break;
  default:  // should not happen
    printf("projb exception: recv unexpected Triad message %d. I'm %s %08x port %d\n", msgtype, Myname, HashID, MyUDPPort);
    break;
  }
  
  return 0;
}

void ClosestPrecedingFinger(unsigned int id, TNode *tn){
  int i = FTLEN - 1;
  for (i = (FTLEN-1); i>0; i--){
    if (InRange(id, HashID, MyFT[i].node.id)){
      tn->id = MyFT[i].node.id;
      tn->port = MyFT[i].node.port;
      return;
    }
  }

  tn->id = HashID;
  tn->port = MyUDPPort;
  return;
}

// return 1 if succeed, 0 if store failed, 2 if string's already stored.
int HandleStoreMsg(int sock, char *str){
  unsigned int str_hash;
  TNode TempA, TempB;
  int msgtype;
  int flag = 0;
  struct sockaddr_in naaddr;
  char  sendbuf[256];
  char  recvbuf[256];
  int nBytestosend;
  int nSendbytes;
  int nRecvbytes;
  STQM  storeq;
  pstrm pstorer;
  int nStrlen;
  int ret;
  
  
  nStrlen = strlen(str);
  str_hash = gethashid(nMgrNonce, str);
  
  if (str_hash > pred.id && str_hash <= HashID){  //store here
    AddClientStore(str_hash, str);
    
    //// just use sendbuf, as it's no more useful
    snprintf(sendbuf, sizeof(sendbuf), "add %s with hash 0x%08x to node 0x%08x\n", str, str_hash, HashID); 
    logfilewriteline(logfilename, sendbuf, strlen(sendbuf));
     
    return 1;
  }else if(str_hash < pred.id && pred.id > HashID && str_hash <= HashID){  // still store here
    // just use sendbuf, as it's no more useful
    
    AddClientStore(str_hash, str);
    snprintf(sendbuf, sizeof(sendbuf), "add %s with hash 0x%08x to node 0x%08x\n", str, str_hash, HashID); 
    logfilewriteline(logfilename, sendbuf, strlen(sendbuf));
     
    return 1;
  }else{ 
    if (nStage < 4){
      if(str_hash < HashID){ // not store here, search start from predecessor
        msgtype = CLSTQ;
        TempA.id = pred.id;
        TempA.port = pred.port;
        flag = 0;
        while(flag == 0){
          if (FindClosest(sock, msgtype, str_hash, TempA, &TempB) < 0){ // it's been changed to stores-q/r messages
            return -1;
          }
      
          if (TempA.id == TempB.id){
            flag = 1;
          }else{
            TempA.id = TempB.id;
            TempA.port = TempB.port;
          }
        }
      }else if(str_hash > HashID){
        msgtype = CLSTQ;
        TempA.id = succ.id;
        TempA.port = succ.port;
        flag = 0;
        while(flag == 0){
          if (FindClosest(sock, msgtype, str_hash, TempA, &TempB) < 0){ // 
            return -1;
          }
      
          if (TempA.id == TempB.id){
            flag = 1;
          }else{
            TempA.id = TempB.id;
            TempA.port = TempB.port;
          }
        }
      }
    }else{
      // find successor
      if (FindSuccWithFT(sock, str_hash, &TempB) < 0){
        printf("projb client %s: HandleStoreMessage FindSuccWithFT fail!\n", Myname);
        return -1;
      }
    }
  }
  
  // We have targeted the destination, now ask it to store the content
  storeq.msgid = htonl(STORQ);
  storeq.ni = htonl(TempB.id);
  storeq.sl = htonl(nStrlen);
  memcpy(sendbuf, &storeq, sizeof(STQM));
  strncpy((sendbuf+sizeof(STQM)), str, nStrlen);
  nBytestosend = sizeof(STQM) + nStrlen;
  
  naaddr.sin_family = AF_INET;
  naaddr.sin_port = htons(TempB.port);
  naaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  
  if ((nSendbytes = sendto(sock, sendbuf, nBytestosend, 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != nBytestosend){
    printf("projb error: HandleStoreMsg sendto ret %d, should send %d\n", nSendbytes, nBytestosend);
    return -1;
  }
  
  LogTyiadMsg(STORQ, SENTFLAG, sendbuf);
  
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
  if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, NULL, NULL)) < 0){
    printf("projb error: HandleStoreMsg recvfrom error.\n" );
    return -1;
  }
  
  pstorer = (pstrm)recvbuf;
  if (ntohl(pstorer->msgid) != STORR || TempB.id != ntohl(pstorer->ni)){
    printf("projb error: HandleStoreMsg recvfrom sanity check fail!\n" );
    return -1;
  }
  
  ret = ntohl(pstorer->r);
  
  LogTyiadMsg(STORR, RECVFLAG, recvbuf);
  
  //log store succeed
  if (ret == 1){
    // just use sendbuf, as it's no more useful
    snprintf(sendbuf, sizeof(sendbuf), "add %s with hash 0x%08x to node 0x%08x\n", str, str_hash, TempB.id); 
    logfilewriteline(logfilename, sendbuf, strlen(sendbuf));
  }
  
  return ret;
}

int HandleSearchMsg(int sock, char *str){
  unsigned int str_hash;
  TNode TempA, TempB;
  int msgtype;
  int flag = 0;
  //struct sockaddr_in naaddr;
  char  szWritebuf[256];
  //int nBytestosend;
  //int nSendbytes;
  //int nRecvbytes;
  //STQM  storeq;
  //pstrm pstorer;
  //int nStrlen = strlen(str);
  int ret;
  
  str_hash = gethashid(nMgrNonce, str);
  
  if (str_hash > pred.id && str_hash <= HashID){  // I should have if, do I?
    if ((ret = SearchClientStore(str_hash, str)) == 1){
      // log 
      snprintf(szWritebuf, sizeof(szWritebuf), "search %s to node 0x%08x, key PRESENT\n", str, HashID); 
      logfilewriteline(logfilename, szWritebuf, strlen(szWritebuf));
      
      return 1;
    }else{
      snprintf(szWritebuf, sizeof(szWritebuf), "search %s to node 0x%08x, key ABSENT\n", str, HashID); 
      logfilewriteline(logfilename, szWritebuf, strlen(szWritebuf));
      return 0;
    }
  }else if(str_hash < pred.id && pred.id > HashID && str_hash <= HashID){  // still store here
    if ((ret = SearchClientStore(str_hash, str)) == 1){
      // log 
      snprintf(szWritebuf, sizeof(szWritebuf), "search %s to node 0x%08x, key PRESENT\n", str, HashID); 
      logfilewriteline(logfilename, szWritebuf, strlen(szWritebuf));
      
      return 1;
    }else{
      snprintf(szWritebuf, sizeof(szWritebuf), "search %s to node 0x%08x, key ABSENT\n", str, HashID); 
      logfilewriteline(logfilename, szWritebuf, strlen(szWritebuf));
      return 0;
    }
  }else if(str_hash < HashID){   // start from predecessor
    msgtype = CLSTQ;
    TempA.id = pred.id;
    TempA.port = pred.port;
    flag = 0;
    while(flag == 0){
      if (nStage >= 4){
        // find succ
        if (FindSuccWithFT(sock, str_hash, &TempA) < 0){
          printf("projb client %s: HandleStoreMessage FindSuccWithFT fail!\n", Myname);
          return -1;
        }
      }

      if ((ret = FindClosest(sock, msgtype, str_hash, TempA, &TempB)) < 0){ // it's been changed to stores-q/r messages
        return -1;
      }
      
      if (TempA.id == TempB.id && ret == 1){  // find the node and it has id, yeah!
        // log and return
        snprintf(szWritebuf, sizeof(szWritebuf), "search %s to node 0x%08x, key PRESENT\n", str, TempA.id); 
        logfilewriteline(logfilename, szWritebuf, strlen(szWritebuf));
        return 1;
      }else if(TempA.id == TempB.id && ret == 2){  // finde the node, but it doesn't have the id.
        snprintf(szWritebuf, sizeof(szWritebuf), "search %s to node 0x%08x, key ABSENT\n", str, TempA.id); 
        logfilewriteline(logfilename, szWritebuf, strlen(szWritebuf));
        return 0;
      }else{
        TempA.id = TempB.id;
        TempA.port = TempB.port;
      }
    }
  }else if(str_hash > HashID){  // start from successor
    msgtype = CLSTQ;
    TempA.id = succ.id;
    TempA.port = succ.port;
    flag = 0;
    while(flag == 0){
      if (nStage >= 4){
        // find succ
        if (FindSuccWithFT(sock, str_hash, &TempA) < 0){
          printf("projb client %s: HandleStoreMessage FindSuccWithFT fail!\n", Myname);
          return -1;
        }
      }

      if ((ret = FindClosest(sock, msgtype, str_hash, TempA, &TempB)) < 0){ // it's been changed to stores-q/r messages
        return -1;
      }
      
      if (TempA.id == TempB.id && ret == 1){  // find the node and it has id, yeah!
        // log and return
        snprintf(szWritebuf, sizeof(szWritebuf), "search %s to node 0x%08x, key PRESENT\n", str, TempA.id); 
        logfilewriteline(logfilename, szWritebuf, strlen(szWritebuf));
        return 1;
      }else if(TempA.id == TempB.id && ret == 2){  // finde the node, but it doesn't have the id.
        snprintf(szWritebuf, sizeof(szWritebuf), "search %s to node 0x%08x, key ABSENT\n", str, TempA.id); 
        logfilewriteline(logfilename, szWritebuf, strlen(szWritebuf));
        return 0;
      }else{
        TempA.id = TempB.id;
        TempA.port = TempB.port;
      }
    }
  }

  // should never get here
  return -1;
}

int HandleEndClient(int sock){
  struct  sockaddr_in naaddr;
  char  sendbuf[128];
  char  recvbuf[128];
  int nSendbytes, nRecvbytes;
  LEQM  lmtosucc;
  LEQM  lmtoother;
  NXRM  nxtosucc;

  naaddr.sin_family = AF_INET;
  naaddr.sin_port = htons(succ.port);
  naaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

  // I am the leaving one, start with sending data to succ
  lmtosucc.msgid = htonl(LEAVQ);
  lmtosucc.ni = htonl(succ.id);
  lmtosucc.di = htonl(HashID);
  memcpy(sendbuf, &lmtosucc, sizeof(LEQM));

  if ((nSendbytes = sendto(sock, sendbuf, sizeof(LEQM), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != sizeof(LEQM)){
    printf("projb %s error: HandleEndClient leaving-q to succ sendto ret %d, should send %lu\n", Myname, nSendbytes, sizeof(LEQM));
    return -1;
  }

  LogTyiadMsg(LEAVQ, SENTFLAG, sendbuf);

  // now successor is going to fetch data
  pCStore storeptr = CStoreHead;
  pnxqm nxqptr;
  int remain = nCStore;
  int slen = 0;
  
  // sending data
  while(1){
    // receive reply
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
    if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(NXQM), 0, NULL, NULL)) != sizeof(NXQM)){
      printf("projb %s error: HandleEndClient recvfrom ret %d, should recv %lu\n", Myname, nRecvbytes, sizeof(NXQM));
      return -1;
    }

    // sanity check
    nxqptr = (pnxqm)recvbuf;
    if (ntohl(nxqptr->msgid) != NXTDQ || ntohl(nxqptr->di) != HashID){
      printf("projb %s error: HandleEndClient next-data-q wrong!\n", Myname);
      return -1;
    }
    LogTyiadMsg(NXTDQ, RECVFLAG, recvbuf);

    
    if (remain == 0){
      // send last and break
      memset(sendbuf, 0, sizeof(sendbuf));
      nxtosucc.di = htonl(HashID);
      nxtosucc.qid = nxqptr->id;
      nxtosucc.rid = htonl(pred.id);
      nxtosucc.sl = htonl(0);
      memcpy(sendbuf, &nxtosucc, sizeof(NXRM));
      if ((nSendbytes = sendto(sock, sendbuf, sizeof(NXRM), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != sizeof(NXRM)){
        printf("projb %s error: HandleEndClient next-data-r ret %d, should send %lu\n", Myname, nSendbytes, sizeof(NXRM));
        return -1;
      }
      LogTyiadMsg(NXTDR, SENTFLAG, sendbuf);
      break;
    }else{
      slen = strlen(storeptr->txt);
      memset(sendbuf, 0, sizeof(sendbuf));
      nxtosucc.di = htonl(HashID);
      nxtosucc.qid = nxqptr->id;
      nxtosucc.rid = htonl(storeptr->id);
      nxtosucc.sl = htonl(slen);
      memcpy(sendbuf, &nxtosucc, sizeof(NXRM));
      memcpy((sendbuf+sizeof(NXRM)), storeptr->txt, slen);

      if ((nSendbytes = sendto(sock, sendbuf, (sizeof(NXRM)+slen), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != (sizeof(NXRM)+slen)){
        printf("projb %s error: HandleEndClient next-data-r ret %d, should send %lu\n", Myname, nSendbytes, (sizeof(NXRM)+slen));
        return -1;
      }

      storeptr = storeptr->next;
      remain--;
    }

    LogTyiadMsg(NXTDR, SENTFLAG, sendbuf);
  }

  // finish sending data, recv leaving-r
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
  if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(LERM), 0, NULL, NULL)) != sizeof(LERM)){
    printf("projb %s error: HandleEndClient recvfrom ret %d, should recv %lu\n", Myname, nRecvbytes, sizeof(LERM));
    return -1;
  }
  LogTyiadMsg(LEAVR, RECVFLAG, recvbuf);

  // now update other finger table
  int i;
  unsigned int nTemp;
  unsigned int exp;
  unsigned int tempFstart;
  TNode tempsu;
  TNode temppr;
  TNode temppr2;
  //TNode myself;

  for (i=1; i<FTLEN; i++){
    exp = (unsigned int)(1<<(i-1));
    nTemp = RingMinus(HashID, exp);

    if(FindSuccWithFT(sock, nTemp, &tempsu) < 0){
      printf("projb client %s: HandleEndClient->FindSuccWithFT fails!\n", Myname);
      return -1;
    }

    if (FindNeighbor(sock, PREDQ, tempsu, &temppr) < 0){
      printf("projb client %s: HandleEndClient->FindNeighbor find predecessor fails!\n", Myname);
      return -1;
    }

    if(temppr.id == HashID || temppr.id == succ.id){
      continue;
    }

    // do it iteratively
    tempFstart = RingPlus(temppr.id, exp);
    while(!NotInRange(tempFstart, pred.id, HashID)){
      pngqm succqptr;
      NGRM  succrmsg;
      // send leaving-q
      naaddr.sin_port = htons(temppr.port);

      lmtoother.msgid = htonl(LEAVQ);
      lmtoother.ni = htonl(temppr.id);
      lmtoother.di = htonl(HashID);
      memcpy(sendbuf, &lmtoother, sizeof(LEQM));
      if ((nSendbytes = sendto(sock, sendbuf, sizeof(LEQM), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != sizeof(LEQM)){
        printf("projb %s error: HandleEndClient leaving-q to other sendto ret %d, should send %lu\n", Myname, nSendbytes, sizeof(LEQM));
        return -1;
      }
      LogTyiadMsg(LEAVQ, SENTFLAG, sendbuf);

      // recv succ-q and reply
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
      if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(NGQM), 0, NULL, NULL)) != sizeof(NGQM)){
        printf("projb %s error: HandleEndClient recv succ-q recvfrom ret %d, shoulde recv %lu\n", Myname, nRecvbytes, sizeof(NGQM));
        return -1;
      }
      succqptr = (pngqm)recvbuf;
      if (ntohl(succqptr->msgid) != SUCCQ){
        printf("projb %s error: HandleEndClient recv succ-q, wrong message recved!\n", Myname);
        return -1;
      }
      LogTyiadMsg(SUCCQ, RECVFLAG, recvbuf);

      succrmsg.msgid = htonl(SUCCR);
      succrmsg.ni = htonl(HashID);
      succrmsg.si = htonl(succ.id);
      succrmsg.sp = htonl((int)(succ.port));
      memset(sendbuf, 0, sizeof(sendbuf));
      memcpy(sendbuf, &succrmsg, sizeof(NGRM));
      if ((nSendbytes = sendto(sock, sendbuf, sizeof(NGRM), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != sizeof(NGRM)){
        printf("projb %s error: HandleEndClient succ-r to other sendto ret %d, should send %lu\n", Myname, nSendbytes, sizeof(NGRM));
        return -1;
      }
      LogTyiadMsg(SUCCR, SENTFLAG, sendbuf);

      // recv leaving-r
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
      if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(LERM), 0, NULL, NULL)) != sizeof(LERM)){
        printf("projb %s error: HandleEndClient update loop leav-r recvfrom ret %d, shoulde recv %lu\n", Myname, nRecvbytes, sizeof(LERM));
        return -1;
      }
      LogTyiadMsg(LEAVR, RECVFLAG, recvbuf);

      // get temppr's predecessor
      if (FindNeighbor(sock, PREDQ, temppr, &temppr2) < 0){
        printf("projb client %s: HandleEndClient->FindNeighbor in loop find predecessor fails!\n", Myname);
        return -1;
      }

      if (temppr2.id == HashID || temppr2.id == succ.id){ // return to myself or my succ, done
        break;
      }

      temppr.id = temppr2.id;
      temppr.port = temppr2.port;

      tempFstart = RingPlus(temppr.id, exp);
    }
  }


  // last, tell succ and pred to update their pred and succ
  if (LeaveUpdateNeighbor(sock, &succ, &pred) < 0){
    printf("projb client %s: HandleEndClient->LeaveUpdateNeighbor fails!\n", Myname);
    return -1;
  }

  return 0;
}

int LeaveUpdateNeighbor(int sock, TNode *chgpreNode, TNode *chgsucNode){
  struct  sockaddr_in naaddr;
  char  sendbuf[128];
  char  recvbuf[128];
  int nSendbytes, nRecvbytes;
  UPQM  updmsg;
  
  // first change someone's predecessor
  naaddr.sin_family = AF_INET;
  naaddr.sin_port = htons(chgpreNode->port);
  naaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  
  
  updmsg.msgid = htonl(UPDTQ);
  updmsg.ni = htonl(chgpreNode->id);
  updmsg.si = htonl(chgsucNode->id);
  updmsg.sp = htonl((int)(chgsucNode->port));
  updmsg.i = htonl(0); // change predecessor
  memcpy(sendbuf, &updmsg, sizeof(UPQM));
  
  if ((nSendbytes = sendto(sock, sendbuf, sizeof(UPQM), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != sizeof(UPQM)){
    printf("projb error: update_neighbor sendto ret %d, should send %lu\n", nSendbytes, sizeof(UPQM));
    return -1;
  }
  
  // log
  LogTyiadMsg(UPDTQ, SENTFLAG, sendbuf);

  // only receive reply after stage 4
  if (nStage >= 2){
    // receive reply
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
    if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(UPRM), 0, NULL, NULL)) != sizeof(UPRM)){
      printf("projb error: update_neighbor recvfrom ret %d, shoulde recv %lu\n", nRecvbytes, sizeof(UPRM));
      return -1;
    }
    LogTyiadMsg(UPDTR, RECVFLAG, recvbuf);
  }
  
  // then, update someone's successor
  naaddr.sin_port = htons(chgsucNode->port);
  
  updmsg.ni = htonl(chgsucNode->id);
  updmsg.si = htonl(chgpreNode->id);
  updmsg.sp = htonl((int)(chgpreNode->port));
  updmsg.i = htonl(1); // update successor
  memset(sendbuf, 0, sizeof(sendbuf));
  memcpy(sendbuf, &updmsg, sizeof(UPQM));
  
  if ((nSendbytes = sendto(sock, sendbuf, sizeof(UPQM), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != sizeof(UPQM)){
    printf("projb error: update_neighbor sendto ret %d, should send %lu\n", nSendbytes, sizeof(UPQM));
    return -1;
  }
  
  // log
  LogTyiadMsg(UPDTQ, SENTFLAG, sendbuf);

  if (nStage >= 2){
    // receive reply
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
    if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(UPRM), 0, NULL, NULL)) != sizeof(UPRM)){
      printf("projb error: update_neighbor recvfrom ret %d, shoulde recv %lu\n", nRecvbytes, sizeof(UPRM));
      return -1;
    }
    LogTyiadMsg(UPDTR, RECVFLAG, recvbuf);
  }
  
  return 0;
}

int SearchClientStore(unsigned int id, char *str){
  pCStore temp = NULL;
  temp = CStoreHead;
  int i;
  
  if (nCStore == 0){
    return 0;
  }else{
    if (str != NULL){
      for (i=0; i<nCStore; i++){
        if (id == temp->id && strcmp(str, temp->txt) == 0)
          return 1;
        else
          temp = temp->next;
      }
    }else{
      for (i=0; i<nCStore; i++){
        if (id == temp->id)
          return 1;
        else
          temp = temp->next;
      }
    }
    return 0;
  }
}

int FindClosest(int sock, int msgt, unsigned int targetid, TNode na, TNode *pnb){  // it's been changed to stores-q/r messages
  CLQM  qrymsg;
  pclrm premsg;
  struct sockaddr_in naaddr;
  char  sendbuf[128];
  char  recvbuf[128];
  int nSendbytes;
  int nRecvbytes;

  // make sure that I don't send message to myself
  if (na.id == HashID){
    // look up in my own finger table
    ClosestPrecedingFinger(targetid, pnb);
    return 0;
  }
  
  qrymsg.msgid = htonl(msgt);
  qrymsg.ni = htonl(na.id);
  qrymsg.di = htonl(targetid);
  memcpy(sendbuf, &qrymsg, sizeof(CLQM));
  
  naaddr.sin_family = AF_INET;
  naaddr.sin_port = htons(na.port);
  naaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  
  if ((nSendbytes = sendto(sock, sendbuf, sizeof(CLQM), 0, (struct sockaddr *)&naaddr, sizeof(naaddr))) != sizeof(CLQM)){
    printf("projb error: find_closest sendto ret %d, should send %lu\n", nSendbytes, sizeof(CLQM));
    return -1;
  }
  
  LogTyiadMsg(msgt, SENTFLAG, sendbuf);
  
  // receive answer
  /**********************************************************************
  *** for stage >= 6, needs to judge if the hello messages comes here ******
  **********************************************************************/
  if ((nRecvbytes = recvfrom(sock, recvbuf, sizeof(CLRM), 0, NULL, NULL)) != sizeof(CLRM)){
    printf("projb error: find_closest recvfrom ret %d, shoulde recv %lu\n", nRecvbytes, sizeof(CLRM));
    return -1;
  }
  
  premsg = (pclrm)recvbuf;
  
  if ((msgt+1) != ntohl(premsg->msgid) || na.id != ntohl(premsg->ni)){
    printf("projb error: find_closest recv ngr msg %d %d, ni %08x %08x\n", msgt, ntohl(premsg->msgid), na.id, ntohl(premsg->ni));
    return -1;
  }
  
  LogTyiadMsg((msgt+1), RECVFLAG, recvbuf);
  
  // following judgement is only used for search
  if (1 == ntohl(premsg->nhas) && na.id == ntohl(premsg->ri)){ // it has the string
    pnb->id = ntohl(premsg->ri);
    pnb->port = ntohl(premsg->rp);
    
    return 1;
  }else if(0 == ntohl(premsg->nhas) && na.id == ntohl(premsg->ri)){ // it doesn't have the string
    pnb->id = ntohl(premsg->ri);
    pnb->port = ntohl(premsg->rp);
    return 2;
  }
  
  pnb->id = ntohl(premsg->ri);
  pnb->port = ntohl(premsg->rp);
  
  return 0;
}

void AddClientStore(unsigned int id, char *str){
  pCStore temp;
  
  if (CStoreHead == NULL){
    CStoreHead = (pCStore)malloc(sizeof(CSTORE));
    CStoreHead->id = id;
    strncpy(CStoreHead->txt, str, MAX_TEXT_SIZE);
    CStoreHead->next = NULL;
    
    nCStore = 1;
  }else{
    temp = (pCStore)malloc(sizeof(CSTORE));
    temp->id = id;
    strncpy(temp->txt, str, MAX_TEXT_SIZE);
    temp->next = CStoreHead;
    CStoreHead = temp;
    nCStore++;
  }
}

void LogTyiadMsg(int mtype, int sorr, char *buf){
  pngqm temp1;
  pngrm temp2;
  pupqm temp3;
  pclqm temp4;
  pclrm temp5;
  pstqm temp6;
  pstrm temp7;
  
  int id, id2, po, nni, nsi, nsp, nflag, ndi, nri, nrp, nsl, nhas;
  int msglen = 0;
  char writebuf[256];
  char  *comtype;
  if (sorr == SENTFLAG) 
    comtype = "sent";
  else if(sorr == RECVFLAG)
    comtype = "received";
  
  switch(mtype){
  case SUCCQ:
    temp1 = (pngqm)buf;
    id = ntohl(temp1->ni);
    snprintf(writebuf, sizeof(writebuf), "successor-q %s (0x%08x)\n", comtype, id);
    logfilewriteline(logfilename, writebuf, strlen(writebuf));
    msglen = sizeof(NGQM);
    break;
  case SUCCR:
    temp2 = (pngrm)buf;
    id = ntohl(temp2->ni);
    id2 = ntohl(temp2->si);
    po = ntohl(temp2->sp);
    snprintf(writebuf, sizeof(writebuf), "successor-r %s (0x%08x 0x%08x %d)\n", comtype, id, id2, po);
    logfilewriteline(logfilename, writebuf, strlen(writebuf));
    msglen = sizeof(NGRM);
    break;
  case PREDQ:
    temp1 = (pngqm)buf;
    id = ntohl(temp1->ni);
    snprintf(writebuf, sizeof(writebuf), "predecessor-q %s (0x%08x)\n", comtype, id);
    logfilewriteline(logfilename, writebuf, strlen(writebuf));
    msglen = sizeof(NGQM);
    break;
  case PREDR:
    temp2 = (pngrm)buf;
    id = ntohl(temp2->ni);
    id2 = ntohl(temp2->si);
    po = ntohl(temp2->sp);
    snprintf(writebuf, sizeof(writebuf), "predecessor-r %s (0x%08x 0x%08x %d)\n", comtype, id, id2, po);
    logfilewriteline(logfilename, writebuf, strlen(writebuf));
    msglen = sizeof(NGRM);
    break;
  case UPDTQ:
    temp3 = (pupqm)buf;
    nni = ntohl(temp3->ni);
    nsi = ntohl(temp3->si);
    nsp = ntohl(temp3->sp);
    nflag = ntohl(temp3->i);
    snprintf(writebuf, sizeof(writebuf), "update-q %s (0x%08x 0x%08x %d %d)\n", comtype, nni, nsi, nsp, nflag);
    logfilewriteline(logfilename, writebuf, strlen(writebuf));
    msglen = sizeof(UPQM);
    break;
  case CLSTQ:
    temp4 = (pclqm)buf;
    nni = ntohl(temp4->ni);
    ndi = ntohl(temp4->di);
    snprintf(writebuf, sizeof(writebuf), "stores-q %s (0x%08x 0x%08x)\n", comtype, nni, ndi);
    logfilewriteline(logfilename, writebuf, strlen(writebuf));
    msglen = sizeof(CLQM);
    break;
  case CLSTR:
    temp5 = (pclrm)buf;
    nni = ntohl(temp5->ni);
    ndi = ntohl(temp5->di);
    nri = ntohl(temp5->ri);
    nrp = ntohl(temp5->rp);
    nhas = ntohl(temp5->nhas);
    snprintf(writebuf, sizeof(writebuf), "stores-r %s (0x%08x 0x%08x 0x%08x %d %d)\n", comtype, nni, ndi, nri, nrp, nhas);
    logfilewriteline(logfilename, writebuf, strlen(writebuf));
    msglen = sizeof(CLRM);
    break;
  case STORQ:
    temp6 = (pstqm)buf;
    nni = ntohl(temp6->ni);
    nsl = ntohl(temp6->sl);
    buf[sizeof(STQM)+nsl] = '\0';
    snprintf(writebuf, sizeof(writebuf), "store-q %s (0x%08x %d %s)\n", comtype, nni, nsl, buf+sizeof(STQM));
    logfilewriteline(logfilename, writebuf, strlen(writebuf));
    msglen = sizeof(STQM)+nsl;
    break;
  case STORR:
    temp7 = (pstrm)buf;
    nni = ntohl(temp7->ni);
    nri = ntohl(temp7->r);
    nsl = ntohl(temp7->sl);
    buf[sizeof(STRM)+nsl] = '\0';
    snprintf(writebuf, sizeof(writebuf), "store-r %s (0x%08x %d %d %s)\n", comtype, nni, nri, nsl, buf+sizeof(STRM));
    logfilewriteline(logfilename, writebuf, strlen(writebuf));
    msglen = sizeof(STRM)+nsl;
    break;
  case UPDTR:
    {
      puprm tempptr = (puprm)buf;
      nni = ntohl(tempptr->ni);
      nsi = ntohl(tempptr->si);
      nsp = ntohl(tempptr->sp);
      nflag = ntohl(tempptr->i);
      int nr = ntohl(tempptr->r);
      snprintf(writebuf, sizeof(writebuf), "update-r %s (0x%08x %d 0x%08x %d %d)\n", comtype, nni, nr, nsi, nsp, nflag);
      logfilewriteline(logfilename, writebuf, strlen(writebuf));
      msglen = sizeof(UPRM);
    }
    break;
  case LEAVQ:
    {
      pleqm tempptr = (pleqm)buf;
      nni = ntohl(tempptr->ni);
      ndi = ntohl(tempptr->di);
      snprintf(writebuf, sizeof(writebuf), "leaving-q %s (0x%08x 0x%08x)\n", comtype, nni, ndi);
      logfilewriteline(logfilename, writebuf, strlen(writebuf));
      msglen = sizeof(LEQM);
    }
    break;
  case LEAVR:
    {
      plerm tempptr = (plerm)buf;
      nni = ntohl(tempptr->ni);
      snprintf(writebuf, sizeof(writebuf), "leaving-r %s (0x%08x)\n", comtype, nni);
      logfilewriteline(logfilename, writebuf, strlen(writebuf));
      msglen = sizeof(LERM);
    }
    break;
  case NXTDQ:
    {
      pnxqm tempptr = (pnxqm)buf;
      ndi = ntohl(tempptr->di);
      id = ntohl(tempptr->id);
      snprintf(writebuf, sizeof(writebuf), "next-data-q %s (0x%08x 0x%08x)\n", comtype, ndi, id);
      logfilewriteline(logfilename, writebuf, strlen(writebuf));
      msglen = sizeof(NXQM);
    }
    break;
  case NXTDR:
    {
      pnxrm tempptr = (pnxrm)buf;
      ndi = ntohl(tempptr->di);
      id = ntohl(tempptr->qid);
      id2 = ntohl(tempptr->rid);
      nsl = ntohl(tempptr->sl);
      buf[sizeof(NXRM)+nsl] = '\0';
      if (nsl!=0){
        snprintf(writebuf, sizeof(writebuf), "next-data-r %s (0x%08x 0x%08x 0x%08x %d %s)\n", comtype, ndi, id, id2, nsl, (buf+sizeof(NXRM)));
      }else{
        snprintf(writebuf, sizeof(writebuf), "next-data-r %s (0x%08x 0x%08x 0x%08x %d)\n", comtype, ndi, id, id2, nsl);
      }
      logfilewriteline(logfilename, writebuf, strlen(writebuf));
      msglen = sizeof(NXRM)+nsl;
    }
    break;
  default:
    break;
  }

  if (strcmp(Myname, "logmsg")==0 && sorr==RECVFLAG){
    int i;
    snprintf(writebuf, sizeof(writebuf), "raw: ");
    //printf("raw: ");
    logfilewriteline(logfilename, writebuf, strlen(writebuf));
    for (i=0; i<msglen; i++){
      //snprintf(writebuf, sizeof(writebuf), "%s%02x", writebuf, buf[i]);
      snprintf(writebuf, sizeof(writebuf), "%02x", ntohl(buf[i]));
      //printf("%02x", ntohl(buf[i]));
      logfilewriteline(logfilename, writebuf, 2*sizeof(char));
    }
    snprintf(writebuf, sizeof(writebuf), "\n");
    //printf("\n");
    //fflush(stdout);
    logfilewriteline(logfilename, writebuf, strlen(writebuf));
  }
}

void LogFingerTable(){
  char wbuf[128];
  int i;

  snprintf(wbuf, sizeof(wbuf), "\n-----------Finger Table----------\n");
  logfilewriteline(logfilename, wbuf, strlen(wbuf));
  
  for (i=0; i<FTLEN; i++){
    snprintf(wbuf, sizeof(wbuf), "|%02d| 0x%08x | 0x%08x | 0x%08x:%05d|\n", i, MyFT[i].start,
      MyFT[i].end, MyFT[i].node.id, MyFT[i].node.port);
    logfilewriteline(logfilename, wbuf, strlen(wbuf));
  }
  snprintf(wbuf, sizeof(wbuf), "---------------------------------\n");
  logfilewriteline(logfilename, wbuf, strlen(wbuf));
}
