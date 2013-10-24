#ifndef FILE_IO_OP_H_
#define FILE_IO_OP_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * This function opens a file in the read only mode
 * @param filename
 * @param mode read/write/append etc modes
 */
FILE* open_file(const char* filename, char* mode);

/**
 * This function reads the line from the stream represented by file pointer fp
 * and store it in the buffer.
 * @param fp
 * @param buff buffer
 * @param size
 */
ssize_t read_line(FILE *fp, char* buff, size_t size);

/**
 * This function writes the data in the file.
 * @param fp the file stream
 * @param buff the character buffer
 */
void write_to_file(FILE *fp, char* buff);

/**
 * This function closes the file
 * @param fp the file stream
 */
void close_file(FILE* fp);

#endif
