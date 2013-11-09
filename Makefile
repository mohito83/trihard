TOPDIR = .
CC = gcc

main: $(TOPDIR)/projb.c $(TOPDIR)/projb.h $(TOPDIR)/manager.c $(TOPDIR)/manager.h $(TOPDIR)/client.c $(TOPDIR)/client.h $(TOPDIR)/comm.h $(TOPDIR)/comm.c $(TOPDIR)/sha1.h $(TOPDIR)/sha1.c $(TOPDIR)/log.h $(TOPDIR)/log.c $(TOPDIR)/ring.h $(TOPDIR)/ring.c
	$(CC) -Wall $(TOPDIR)/projb.c $(TOPDIR)/manager.c $(TOPDIR)/client.c $(TOPDIR)/comm.c $(TOPDIR)/sha1.c $(TOPDIR)/log.c $(TOPDIR)/ring.c -o $(TOPDIR)/projb

clean:
	rm -rf projb
