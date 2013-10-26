
/*
 * name_to_id.c
 * Copyright (C) 2011 by USC/ISI
 * $Id: name_to_id_partial.c 22 2011-09-28 14:27:03Z johnh $
 *
 * Copyright (c) 2011 University of Southern California.
 * All rights reserved.                                            
 *                                                                
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation, advertising
 * materials, and other materials related to such distribution and use
 * acknowledge that the software was developed by the University of
 * Southern California, Information Sciences Institute.  The name of the
 * University may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * 
 */

/* 
 * code to test class function
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sha1.h"

void
die(char *s)
{
	fprintf(stderr, "%s\n", s);
	exit(1);
};


/*
 * Each student needs to complete this function
 * using the class-provided projb_hash.
 *
 * If you build this file
 *    gcc -o name_to_id_partial name_to_id.c sha1.c
 */
unsigned int
nonce_name_hash(int nonce, char *name)
{
	int name_length = strlen(name);
	int buffer_length = sizeof(int) + name_length;
	unsigned char *buffer = malloc(buffer_length);

	die("you need to fill in this part of the function to build\n the buffer as per the project specification.\n");

	unsigned int result = projb_hash(buffer, buffer_length);
	free(buffer);

	return result;
}


int
main(int argc, char **argv)
{
	uint32_t hash;
	int nonce;
	char *name;

	if (argc != 3)
		die("usage: nonce string");
	nonce = atoi(argv[1]);
	name = argv[2];

	hash = nonce_name_hash(nonce, name);
	printf("(%d, %s) => %x\n", nonce, name, hash);

  return 0;
}
