CC = gcc -Wall
PF = -lpthread
FILES = echocl_nb.c

client:
	$(CC) $(FILES) -o echocl_nb $(PF)
 

CFLAGS = -static -std=c99 -D_GNU_SOURCE=1

other: nagle echocl echocl_nb

clean_all:
	rm nagle echocl echocl_nb
