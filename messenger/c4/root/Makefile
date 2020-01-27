# Makefile for server
CC = gcc
STD = -std=gnu99

RFLAGS = -Wall 
DFLAGS = -Wall -g3
# Postfix
PF = -lrt -lpthread -lresolv
# Common
CF = -D_XOPEN_SOURCE
# All files
FILES = # -o echosrv.c  sockets.c smtp.c logger.c common.c process.c handlers.c

all: clean release win

release: server_r
debug: server_d

# Launch server
server_r:
	$(CC) $(STD) $(RFLAGS) $(FILES) echosrv.c -o echosrv $(PF)

server_d:
	$(CC) $(STD) $(DFLAGS) $(FILES) echosrv.c -o echosrv $(PF)

clean:
	rm -rf *.o echosrv


# CFLAGS = -std=c99 -D_GNU_SOURCE=1

win: 
	clean_win win_run

win_run:
	gcc -std=c99 -Wall  win.c -o win

clean_win:
	rm win

