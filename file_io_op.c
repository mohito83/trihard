#include "file_io_op.h"

/**
 * This function is used to open a file.
 * @ return fp the file pointer
 */
FILE* open_file(const char* filename, char* mode) {
	FILE* fp;
	fp = fopen(filename, mode);
	if (fp < 0) {
		perror("unable to open the file!!\n");
	}
	return fp;
}

/**
 * This function reads the line from the file represented by file pointer fp
 * and store it in the buffer.
 * @param fp
 * @param buff buffer
 */
ssize_t read_line(FILE *file, char* buff, size_t size) {
	if (file != NULL) {
		ssize_t r_code = getline(&buff, &size, file);

		if (r_code == -1 /*&& !feof(file)*/) {
			/*perror("Error reading a line of data from the file!!\n");*/
			return 0;
		} else {
			return 1;
		}
	} else {
		return 0;
	}
}

/**
 * This function writes the data in the file.
 * @param fp the file pointer
 * @param buff the character buffer
 */
void write_to_file(FILE* fp, char* buff) {
	if (fp != NULL) {
		int r_code = fputs(buff, fp);
		fprintf(fp,"\n");
		if (r_code == EOF) {
			perror("Error writing data to file!!\n");
		}
	}
}

/**
 * This function closes the file
 * @param fp the file stream
 */
void close_file(FILE* fp) {
	if (fp > 0) {
		fclose(fp);
	}
}

