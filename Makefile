projb: file_io_op.o sock_op.o manager.o client.o
	gcc -Wall -g -o proja file_io_op.o sock_op.o manager.o client.o
	
manager.o: manager.c file_io_op.h client.h
	gcc -Wall -g -c manager.c

file_io_op.o: file_io_op.c
	gcc -Wall -g -c file_io_op.c

sock_op.o: sock_op.c
	gcc -Wall -g -c sock_op.c
	
client.o: client.c
	gcc -Wall -g -c client.c
	
clean:
	rm *.o projb

