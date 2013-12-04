all: timer main
	g++ -o projc projb.o manager.o client.o comm.o sha1.o log.o ring.o timers-c.o timers.o tools.o
timer:
	gcc -c -Wall timers-c.cc timers.cc tools.cc
main:
	gcc -c -Wall projb.c manager.c client.c comm.c sha1.c log.c ring.c
	
clean:
	rm -rf projc *.o *.out