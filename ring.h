#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define HASHMAX 0xFFFFFFFF

int InRange(unsigned int id, unsigned int na, unsigned int nb);
int InRangeA(unsigned int n, unsigned int start, unsigned int node);
int NotInRange(unsigned int id, unsigned int ia, unsigned int ib);

unsigned int RingPlus(unsigned int a, unsigned int b);
unsigned int RingMinus(unsigned int a, unsigned int b);
