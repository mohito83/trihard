#include "log.h"

void logfileinit(char *filename)
{
  int fd;
  fd = creat(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (fd < 0)
    printf("projb manager error: create log file error.\n");
  close(fd);
}

void logfilewriteline(char *filename, char *logbuf, int length)
{
  int fd;
  fd = open(filename, O_WRONLY | O_APPEND);
  write(fd, logbuf, length);
  fsync(fd);
  close(fd);
}

