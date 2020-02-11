CC = gcc
STD = -std=gnu99

RFLAGS = -Wall -Werr -O2
DFLAGs = -Wall -Werr -O2 -g3
PF = -lrt -lpthread -lresolv
#common ??
FILES = mta_server.c
EXE = mta_server

all: clean release # debug ? test ?

release: 
	$(CC) $(STD) $(RFLAGS) $(FILES) -o $(EXE) $(PF)

debug:
	$(CC) $(STD) $(DFLAGS) $(FILES) -o $(EXE) $(PF)


clean:
	rm -rf *.o $(EXE)