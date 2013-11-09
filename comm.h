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
 
// File name: 	comm.h
// Author: 		Xun Fan (xunfan@usc.edu)
// Date: 		2011.8
// Description: CSCI551 fall 2011 project b, communication module header file.

#ifndef _COMM_H
#define _COMM_H

#define SELECT_TIMEOUT 3600  //60min

/**
 * This function reads in <code>length</code> bytes from
 * the socket file descriptor.
 * 
 * Returns 0 on success and -1 on error.
 */
int SendStreamData(int sock, char *buf, int length);

/**
 * This function reads a line from the socket file descriptor.
 * 
 * Returns received buffer size on success or negative integer
 * on error.
 */
int RecvStreamLineForSelect(int sock, char *buf, int bufsize);

#endif
