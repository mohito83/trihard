projb: file_io_op.o sock_op.o manager.o client.o sha1.o
	gcc -Wall -g -o projb file_io_op.o sock_op.o sha1.o manager.o client.o
	
manager.o: manager.c file_io_op.h client.h
	gcc -Wall -g -c manager.c

file_io_op.o: file_io_op.c
	gcc -Wall -g -c file_io_op.c

sock_op.o: sock_op.c
	gcc -Wall -g -c sock_op.c
	
client.o: client.c sha1.h
	gcc -Wall -g -c client.c
	
sha1.o: sha1.c
	 gcc -Wall -c sha1.c
	
clean:
	rm *.o projb

