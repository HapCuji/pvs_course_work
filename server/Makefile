CC = gcc
STD = -std=gnu99

RFLAGS = -Wall -Werror -O2
DFLAGs = -Wall -Werror -O2 -g3
PF = -lrt -lpthread -lresolv
TEST_PF = #-lrt -lpthread -lresolv -lcunit
#common ??
FILES = mta_server.c mta_thread.c mta_process.c # mta_communication.c
EXE = mta_server
TEST_FILES = test_metod_mta.c
TEST_EXE = test_mta

all: clean release test # debug ? 

release: 
	$(CC) $(STD) $(RFLAGS) $(FILES) -o $(EXE) $(PF)

debug:
	$(CC) $(STD) $(DFLAGS) $(FILES) -o $(EXE) $(PF)

test:
	$(CC) $(STD) $(RFLAGS) $(FILES) $(TEST_FILES) -o $(TEST_EXE) $(TEST_PF) 

clean:
	rm -rf *.o $(EXE) $(TEST_EXE) 